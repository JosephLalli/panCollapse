#include <handlegraph/path_position_handle_graph.hpp>
#include <handlegraph/util.hpp>
#include <htslib/sam.h>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include "pathtally.hpp"
#include "pathtally_ledger.hpp"
#include "pathtally_qualadj.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr uint8_t kRadU8 = 1;
constexpr uint8_t kRadU16 = 2;
constexpr uint8_t kRadU32 = 3;
constexpr uint8_t kRadU64 = 4;
constexpr uint32_t kRadForwardMask = 0x80000000U;
constexpr uint32_t kRadTargetIdMask = 0x7fffffffU;

// Alignment-slop guard for the ledger spliced/unspliced/ambiguous classification (velocyto MIN_FLANK,
// STARsolo minOverlapMinusOne). A transcript still counts as explaining a read exon-only if the read's
// aligned bases fall within this many of the read's top score; a read counts as touching a gene's
// constitutive intron only past this many body-only bases. Tunable on the d46 debug fixture.
constexpr int64_t kIntronFlankBases = 5;

enum class MoleculeIdentityFailurePolicy { Skip, Fail };

enum class ScoreMode { Flat, QualAdj };

// XT-tag policy for reads compatible with more than one gene. Omit skips XT (a --per-gene
// counter then ignores the read); First writes XT for the first gene in sorted order; All is a
// ledger-count-mode-only policy that additionally carries the read into the BAM at all (it is
// otherwise hard-dropped from both RAD and BAM for the ledger --count-mode Unique rule), tagged
// with its full candidate-gene GX set and no XT, so a downstream UMI-level rescue
// (STARsolo/CellRanger MultiGeneUMI_CR) can resolve the dominant gene. All has no effect in
// --count-mode score, where multi-target reads are ordinary D048 multimapping evidence, already
// emitted to both RAD and BAM.
enum class BamMultiGenePolicy { Omit, First, All };

// Target-relative orientation filter. Both keeps every compatible target (default, no
// filtering). Forward keeps only targets the read aligns to in the same orientation (sense);
// Reverse keeps only antisense targets. Orientation is the majority of aligned bases, as in
// the RAD dirs.
enum class StrandFilter { Both, Forward, Reverse };

enum class MoleculeParseStatus { Ok, Missing, Malformed, Unsupported };

struct Options {
    std::filesystem::path gamp;
    std::filesystem::path xg;
    std::filesystem::path t2g;
    std::filesystem::path out_dir;
    size_t raw_cb_length = 16;
    size_t raw_umi_length = 12;
    MoleculeIdentityFailurePolicy molecule_identity_failures = MoleculeIdentityFailurePolicy::Skip;
    ScoreMode score_mode = ScoreMode::Flat;
    std::filesystem::path bam_out;  // empty => no BAM output
    BamMultiGenePolicy bam_multigene = BamMultiGenePolicy::Omit;
    StrandFilter strand = StrandFilter::Both;
    pathtally::CountMode count_mode = pathtally::CountMode::Score;
    std::filesystem::path body_t2g;  // gene-body layer t2g, required for ledger count modes
};

struct MoleculeId {
    std::string original_name;
    std::string barcode;
    std::string umi;
};

struct MoleculeParseResult {
    MoleculeParseStatus status = MoleculeParseStatus::Ok;
    MoleculeId id;
    std::string message;
};

struct TargetHit {
    uint32_t target_id = 0;
    bool is_forward = true;
};

struct EmittedRecord {
    MoleculeId molecule;
    std::vector<TargetHit> hits;
};

// The transcript-to-gene map (t2g): HST_name -> gene. PathTally collapses HST
// haplotype copies to a transcript id; the RAD dictionary is the sorted set of
// those transcript ids.
struct T2gData {
    std::unordered_set<std::string> hst_names;
    std::map<std::string, std::string> transcript_gene;
    std::map<std::string, uint32_t> target_ids;
    std::vector<std::string> target_names;
    // Sorted unique gene ids and gene -> index, used only for the optional BAM output:
    // one @SQ contig per gene, in this order.
    std::vector<std::string> gene_names;
    std::map<std::string, int32_t> gene_ids;
};

constexpr const char* kUsageText =
    "usage: panCollapse convert --gamp reads.gamp|- --xg graph.xg --t2g t2g.tsv "
    "--out-dir out [--raw-cb-length N] [--raw-umi-length N] "
    "[--score flat|qualadj] [--molecule-identity-failures skip|fail] "
    "[--strand both|forward|reverse] "
    "[--count-mode score|gene|genefull|genefull_exonoverintron|genefull_ex50pas --body-t2g body.t2g] "
    "[--bam-out reads.bam] [--bam-multigene omit|first|all]";

[[noreturn]] void usage_error() {
    throw std::runtime_error(kUsageText);
}

size_t parse_size_option(const std::string& option_name, const std::string& value) {
    if (value.empty()) {
        throw std::runtime_error(option_name + " must be an unsigned integer");
    }
    size_t parsed = 0;
    for (const char c : value) {
        if (c < '0' || c > '9') {
            throw std::runtime_error(option_name + " must be an unsigned integer");
        }
        const size_t digit = static_cast<size_t>(c - '0');
        if (parsed > (std::numeric_limits<size_t>::max() - digit) / 10) {
            throw std::runtime_error(option_name + " is outside the supported integer range");
        }
        parsed = parsed * 10 + digit;
    }
    return parsed;
}

Options parse_options(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) != "convert") {
        usage_error();
    }

    Options options;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const std::string& name) -> std::string {
            if (arg != name || i + 1 >= argc) {
                usage_error();
            }
            return argv[++i];
        };

        if (arg == "--gamp") {
            options.gamp = require_value("--gamp");
        } else if (arg == "--xg") {
            options.xg = require_value("--xg");
        } else if (arg == "--t2g") {
            options.t2g = require_value("--t2g");
        } else if (arg == "--out-dir") {
            options.out_dir = require_value("--out-dir");
        } else if (arg == "--raw-cb-length") {
            options.raw_cb_length = parse_size_option("--raw-cb-length", require_value("--raw-cb-length"));
        } else if (arg == "--raw-umi-length") {
            options.raw_umi_length = parse_size_option("--raw-umi-length", require_value("--raw-umi-length"));
        } else if (arg == "--score") {
            const std::string value = require_value("--score");
            if (value == "flat") {
                options.score_mode = ScoreMode::Flat;
            } else if (value == "qualadj") {
                options.score_mode = ScoreMode::QualAdj;
            } else {
                usage_error();
            }
        } else if (arg == "--molecule-identity-failures") {
            const std::string value = require_value("--molecule-identity-failures");
            if (value == "skip") {
                options.molecule_identity_failures = MoleculeIdentityFailurePolicy::Skip;
            } else if (value == "fail") {
                options.molecule_identity_failures = MoleculeIdentityFailurePolicy::Fail;
            } else {
                usage_error();
            }
        } else if (arg == "--strand") {
            const std::string value = require_value("--strand");
            if (value == "both") {
                options.strand = StrandFilter::Both;
            } else if (value == "forward") {
                options.strand = StrandFilter::Forward;
            } else if (value == "reverse") {
                options.strand = StrandFilter::Reverse;
            } else {
                usage_error();
            }
        } else if (arg == "--count-mode") {
            const std::string value = require_value("--count-mode");
            if (value == "score") {
                options.count_mode = pathtally::CountMode::Score;
            } else if (value == "gene") {
                options.count_mode = pathtally::CountMode::Gene;
            } else if (value == "genefull") {
                options.count_mode = pathtally::CountMode::GeneFull;
            } else if (value == "genefull_exonoverintron") {
                options.count_mode = pathtally::CountMode::GeneFullExonOverIntron;
            } else if (value == "genefull_ex50pas") {
                options.count_mode = pathtally::CountMode::GeneFullEx50pAS;
            } else {
                usage_error();
            }
        } else if (arg == "--body-t2g") {
            options.body_t2g = require_value("--body-t2g");
        } else if (arg == "--bam-out") {
            options.bam_out = require_value("--bam-out");
        } else if (arg == "--bam-multigene") {
            const std::string value = require_value("--bam-multigene");
            if (value == "omit") {
                options.bam_multigene = BamMultiGenePolicy::Omit;
            } else if (value == "first") {
                options.bam_multigene = BamMultiGenePolicy::First;
            } else if (value == "all") {
                options.bam_multigene = BamMultiGenePolicy::All;
            } else {
                usage_error();
            }
        } else {
            usage_error();
        }
    }

    if (options.gamp.empty() || options.xg.empty() || options.t2g.empty() || options.out_dir.empty()) {
        usage_error();
    }
    if (options.raw_cb_length == 0 || options.raw_cb_length > 32 || options.raw_umi_length == 0 ||
        options.raw_umi_length > 32) {
        throw std::runtime_error("raw barcode and UMI lengths must be in 1..32");
    }
    // Ledger count modes read two path->gene t2gs: --t2g is the exon/HST layer, --body-t2g the
    // gene-body layer. --body-t2g is meaningless without a ledger mode.
    if (options.count_mode == pathtally::CountMode::Score) {
        if (!options.body_t2g.empty()) {
            throw std::runtime_error("--body-t2g is only used with a ledger --count-mode");
        }
    } else if (options.body_t2g.empty()) {
        throw std::runtime_error("--count-mode gene/genefull/... requires --body-t2g");
    }

    return options;
}

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

T2gData read_t2g(const std::filesystem::path& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open t2g");
    }
    T2gData data;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto fields = split_tab(line);
        if (fields.size() < 2 || fields[0].empty() || fields[1].empty()) {
            throw std::runtime_error("t2g row must have transcript and gene columns");
        }
        const std::string& hst_name = fields[0];
        const std::string& gene = fields[1];
        data.hst_names.insert(hst_name);
        const std::string transcript = pathtally::transcript_id_of(hst_name);
        auto [it, inserted] = data.transcript_gene.emplace(transcript, gene);
        if (!inserted && it->second != gene) {
            throw std::runtime_error("t2g maps transcript " + transcript + " to multiple genes");
        }
    }
    if (data.transcript_gene.empty()) {
        throw std::runtime_error("t2g has no data rows");
    }
    if (data.transcript_gene.size() > kRadTargetIdMask) {
        throw std::runtime_error("RAD target dictionary exceeds 31-bit target IDs");
    }
    for (const auto& [transcript, gene] : data.transcript_gene) {
        data.target_ids[transcript] = static_cast<uint32_t>(data.target_names.size());
        data.target_names.push_back(transcript);
        // std::map keeps genes sorted and unique; emplace assigns each its @SQ index in order.
        data.gene_ids.emplace(gene, 0);
    }
    for (auto& [gene, id] : data.gene_ids) {
        id = static_cast<int32_t>(data.gene_names.size());
        data.gene_names.push_back(gene);
    }
    return data;
}

// Read a t2g as a raw path_name -> gene map (no transcript-id collapse). Used for the ledger
// count modes, where the exon (HST) and gene-body layers are each an ordinary path->gene t2g.
std::map<std::string, std::string> read_path_gene(const std::filesystem::path& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open t2g " + filename.string());
    }
    std::map<std::string, std::string> path_gene;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto fields = split_tab(line);
        if (fields.size() < 2 || fields[0].empty() || fields[1].empty()) {
            throw std::runtime_error("t2g row must have path and gene columns");
        }
        auto [it, inserted] = path_gene.emplace(fields[0], fields[1]);
        if (!inserted && it->second != fields[1]) {
            throw std::runtime_error("t2g maps path " + fields[0] + " to multiple genes");
        }
    }
    if (path_gene.empty()) {
        throw std::runtime_error("t2g has no data rows: " + filename.string());
    }
    return path_gene;
}

bool is_supported_molecule_base(char base) {
    switch (std::toupper(static_cast<unsigned char>(base))) {
    case 'A':
    case 'C':
    case 'G':
    case 'T':
    case 'N':
        return true;
    default:
        return false;
    }
}

std::string molecule_status_counter(MoleculeParseStatus status) {
    switch (status) {
    case MoleculeParseStatus::Missing:
        return "raw_molecule_missing_groups";
    case MoleculeParseStatus::Malformed:
        return "raw_molecule_malformed_groups";
    case MoleculeParseStatus::Unsupported:
        return "raw_molecule_unsupported_groups";
    case MoleculeParseStatus::Ok:
        break;
    }
    throw std::runtime_error("unknown molecule parse status");
}

MoleculeParseResult parse_molecule_id(const std::string& name, size_t cb_length, size_t umi_length) {
    const size_t umi_sep = name.rfind('_');
    if (umi_sep == std::string::npos) {
        return {MoleculeParseStatus::Missing, {}, "GAMP name does not contain raw UMI"};
    }
    if (umi_sep == 0 || umi_sep + 1 == name.size()) {
        return {MoleculeParseStatus::Missing, {}, "GAMP name has an empty raw CB or UMI field"};
    }
    const size_t cb_sep = name.rfind('_', umi_sep - 1);
    if (cb_sep == std::string::npos) {
        return {MoleculeParseStatus::Missing, {}, "GAMP name does not contain raw CB"};
    }

    MoleculeId id{name.substr(0, cb_sep), name.substr(cb_sep + 1, umi_sep - cb_sep - 1), name.substr(umi_sep + 1)};
    if (id.barcode.empty() || id.umi.empty()) {
        return {MoleculeParseStatus::Missing, {}, "GAMP name has an empty raw CB or UMI field"};
    }
    if (id.original_name.empty()) {
        return {MoleculeParseStatus::Malformed, {}, "GAMP name has no original read-name prefix"};
    }
    if (id.barcode.size() != cb_length || id.umi.size() != umi_length) {
        return {MoleculeParseStatus::Malformed, {}, "raw CB/UMI lengths do not match configured lengths"};
    }
    for (const char base : id.barcode) {
        if (!is_supported_molecule_base(base)) {
            return {MoleculeParseStatus::Unsupported, {}, "raw CB contains unsupported base"};
        }
    }
    for (const char base : id.umi) {
        if (!is_supported_molecule_base(base)) {
            return {MoleculeParseStatus::Unsupported, {}, "raw UMI contains unsupported base"};
        }
    }
    return {MoleculeParseStatus::Ok, id, {}};
}

uint8_t base_bits(char base) {
    switch (std::toupper(static_cast<unsigned char>(base))) {
    case 'A':
    case 'N':
        return 0;
    case 'C':
        return 1;
    case 'G':
        return 2;
    case 'T':
        return 3;
    default:
        throw std::runtime_error("raw CB/UMI contains unsupported base");
    }
}

uint64_t pack_sequence(const std::string& sequence) {
    if (sequence.empty() || sequence.size() > 32) {
        throw std::runtime_error("cannot pack sequence length outside 1..32");
    }
    uint64_t packed = 0;
    for (const char base : sequence) {
        packed = (packed << 2) | base_bits(base);
    }
    return packed;
}

uint8_t rad_int_type_for_length(size_t length) {
    if (length <= 4) return kRadU8;
    if (length <= 8) return kRadU16;
    if (length <= 16) return kRadU32;
    if (length <= 32) return kRadU64;
    throw std::runtime_error("RAD cannot encode raw CB/UMI length greater than 32");
}

template <typename T> void write_le(std::ostream& out, T value) {
    using U = std::make_unsigned_t<T>;
    U v = static_cast<U>(value);
    for (size_t i = 0; i < sizeof(T); ++i) {
        out.put(static_cast<char>((v >> (8 * i)) & 0xffU));
    }
    if (!out) {
        throw std::runtime_error("failed to write RAD file");
    }
}

void write_string_with_u16_length(std::ostream& out, const std::string& value) {
    if (value.size() > UINT16_MAX) {
        throw std::runtime_error("RAD string is too long");
    }
    write_le<uint16_t>(out, static_cast<uint16_t>(value.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!out) {
        throw std::runtime_error("failed to write RAD string");
    }
}

void write_tag(std::ostream& out, const std::string& name, uint8_t type_id) {
    write_string_with_u16_length(out, name);
    write_le<uint8_t>(out, type_id);
}

void write_packed_value(std::ostream& out, uint8_t type_id, uint64_t value) {
    switch (type_id) {
    case kRadU8:
        write_le<uint8_t>(out, static_cast<uint8_t>(value));
        break;
    case kRadU16:
        write_le<uint16_t>(out, static_cast<uint16_t>(value));
        break;
    case kRadU32:
        write_le<uint32_t>(out, static_cast<uint32_t>(value));
        break;
    case kRadU64:
        write_le<uint64_t>(out, value);
        break;
    default:
        throw std::runtime_error("unsupported RAD integer type");
    }
}

size_t rad_int_width(uint8_t type_id) {
    switch (type_id) {
    case kRadU8:
        return 1;
    case kRadU16:
        return 2;
    case kRadU32:
        return 4;
    case kRadU64:
        return 8;
    default:
        throw std::runtime_error("unsupported RAD integer type");
    }
}

// Streams RAD to disk: writes the header and tag sections up front (the target
// dictionary comes from the t2g, known before any record), then each read record
// as it is produced. Records are split into chunks so no chunk's byte count or
// record count exceeds the u32 fields the format uses; each chunk header and the
// file-level num_chunks are seek-and-backpatched once their counts are known. Only
// the current record is ever held.
//
// The file is written to a temporary path (<final_path>.tmp) in the same directory
// and atomically renamed to the final path only after finalize() completes. If the
// run aborts before finalize(), no map.rad is left on disk; the destructor removes
// the temp file as a best-effort cleanup.
class RadStreamWriter {
public:
    RadStreamWriter(const std::filesystem::path& filename, const std::vector<std::string>& target_names,
                    size_t cb_length, size_t umi_length, uint64_t max_chunk_bytes)
        : final_path_(filename),
          temp_path_(filename.string() + ".tmp"),
          out_(temp_path_, std::ios::binary),
          cb_type_(rad_int_type_for_length(cb_length)),
          umi_type_(rad_int_type_for_length(umi_length)),
          read_value_size_(rad_int_width(cb_type_) + rad_int_width(umi_type_)),
          max_chunk_bytes_(max_chunk_bytes) {
        if (!out_) {
            throw std::runtime_error("cannot write RAD output");
        }
        write_le<uint8_t>(out_, 0);
        write_le<uint64_t>(out_, static_cast<uint64_t>(target_names.size()));
        for (const std::string& target_name : target_names) {
            write_string_with_u16_length(out_, target_name);
        }
        num_chunks_pos_ = out_.tellp();
        write_le<uint64_t>(out_, 0);  // num_chunks placeholder, patched in finalize()

        write_le<uint16_t>(out_, 2);
        write_tag(out_, "cblen", kRadU16);
        write_tag(out_, "ulen", kRadU16);
        write_le<uint16_t>(out_, 2);
        write_tag(out_, "b", cb_type_);
        write_tag(out_, "u", umi_type_);
        write_le<uint16_t>(out_, 1);
        write_tag(out_, "compressed_ori_refid", kRadU32);
        write_le<uint16_t>(out_, static_cast<uint16_t>(cb_length));
        write_le<uint16_t>(out_, static_cast<uint16_t>(umi_length));
    }

    ~RadStreamWriter() {
        if (!finalized_) {
            out_.close();
            std::error_code ec;
            std::filesystem::remove(temp_path_, ec);
        }
    }

    void write_record(const MoleculeId& molecule, const std::vector<TargetHit>& hits) {
        if (hits.empty()) {
            throw std::runtime_error("cannot write RAD record with no target hits");
        }
        const uint64_t record_bytes = 4 + read_value_size_ + 4 * static_cast<uint64_t>(hits.size());
        if (record_bytes + 8 > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("RAD record is too large to encode in one chunk");
        }
        // Roll to a new chunk before this record would push a non-empty chunk past
        // the limit, so every chunk holds >=1 whole record and stays under u32.
        if (chunk_open_ && chunk_nrec_ > 0 && chunk_bytes_ + record_bytes > max_chunk_bytes_) {
            close_chunk();
        }
        if (!chunk_open_) {
            open_chunk();
        }

        write_le<uint32_t>(out_, static_cast<uint32_t>(hits.size()));
        write_packed_value(out_, cb_type_, pack_sequence(molecule.barcode));
        write_packed_value(out_, umi_type_, pack_sequence(molecule.umi));
        for (const TargetHit& hit : hits) {
            if (hit.target_id > kRadTargetIdMask) {
                throw std::runtime_error("RAD target ID exceeds 31-bit encoding limit");
            }
            write_le<uint32_t>(out_, hit.target_id | (hit.is_forward ? kRadForwardMask : 0));
        }
        chunk_bytes_ += record_bytes;
        ++chunk_nrec_;
        ++total_records_;
        if (chunk_nrec_ == std::numeric_limits<uint32_t>::max()) {
            close_chunk();
        }
    }

    uint64_t record_count() const { return total_records_; }

    void finalize() {
        close_chunk();
        out_.seekp(num_chunks_pos_);
        write_le<uint64_t>(out_, num_chunks_);
        out_.flush();
        if (!out_) {
            throw std::runtime_error("failed to finalize RAD file");
        }
        out_.close();
        std::filesystem::rename(temp_path_, final_path_);
        finalized_ = true;
    }

private:
    void open_chunk() {
        chunk_header_pos_ = out_.tellp();
        write_le<uint32_t>(out_, 0);  // chunk nbytes placeholder
        write_le<uint32_t>(out_, 0);  // chunk nrec placeholder
        chunk_bytes_ = 8;             // the 8-byte chunk header
        chunk_nrec_ = 0;
        chunk_open_ = true;
    }

    void close_chunk() {
        if (!chunk_open_) {
            return;
        }
        if (chunk_bytes_ > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("RAD chunk is too large");
        }
        const std::streampos end = out_.tellp();
        out_.seekp(chunk_header_pos_);
        write_le<uint32_t>(out_, static_cast<uint32_t>(chunk_bytes_));
        write_le<uint32_t>(out_, chunk_nrec_);
        out_.seekp(end);
        ++num_chunks_;
        chunk_open_ = false;
    }

    std::filesystem::path final_path_;
    std::filesystem::path temp_path_;
    std::ofstream out_;
    uint8_t cb_type_;
    uint8_t umi_type_;
    size_t read_value_size_;
    uint64_t max_chunk_bytes_;
    std::streampos num_chunks_pos_{};
    std::streampos chunk_header_pos_{};
    bool chunk_open_ = false;
    uint32_t chunk_nrec_ = 0;
    uint64_t chunk_bytes_ = 8;
    uint64_t num_chunks_ = 0;
    uint64_t total_records_ = 0;
    bool finalized_ = false;
};

// Optional BAM output for a CellRanger-style counting stack (UMI-tools count --per-gene
// --gene-tag, then DropletUtils emptyDropsCellRanger). The RAD remains the primary output;
// this writer is only constructed when --bam-out is given.
//
// The BAM carries panCollapse's graph-derived gene assignment as 10x tags, not real
// alignment coordinates: UMI-tools dedups by (cell, UMI, gene) and ignores position, so each
// read is placed nominally at position 1 of a synthetic per-gene contig. This keeps reads
// that would never surject to the linear genome (non-reference alleles/insertions), which is
// the whole point of counting off panCollapse instead of a genome-surjected BAM. One record
// per emitted read, always flagged mapped. Records are written in GAMP order (@HD SO:unsorted);
// downstream must samtools sort + index before umi_tools count.
class BamWriter {
public:
    // One synthetic contig per gene needs a length; the value is nominal (position is ignored
    // downstream) but must exceed any read length so a POS=1 full-length M CIGAR stays valid.
    static constexpr int32_t kNominalContigLength = 1 << 28;

    BamWriter(const std::filesystem::path& filename, const std::vector<std::string>& gene_names,
              const std::string& version, const std::string& command_line)
        : final_path_(filename), temp_path_(filename.string() + ".tmp") {
        fp_ = hts_open(temp_path_.c_str(), "wb");
        if (fp_ == nullptr) {
            throw std::runtime_error("cannot open BAM output");
        }
        hdr_ = sam_hdr_init();
        rec_ = bam_init1();
        if (hdr_ == nullptr || rec_ == nullptr) {
            throw std::runtime_error("cannot allocate BAM header/record");
        }
        if (sam_hdr_add_line(hdr_, "HD", "VN", "1.6", "SO", "unsorted", NULL) < 0) {
            throw std::runtime_error("cannot write BAM @HD line");
        }
        const std::string length = std::to_string(kNominalContigLength);
        for (const std::string& gene : gene_names) {
            if (sam_hdr_add_line(hdr_, "SQ", "SN", gene.c_str(), "LN", length.c_str(), NULL) < 0) {
                throw std::runtime_error("cannot write BAM @SQ line for gene " + gene);
            }
        }
        if (sam_hdr_add_line(hdr_, "PG", "ID", "panCollapse", "PN", "panCollapse", "VN",
                             version.c_str(), "CL", command_line.c_str(), NULL) < 0) {
            throw std::runtime_error("cannot write BAM @PG line");
        }
        if (sam_hdr_write(fp_, hdr_) < 0) {
            throw std::runtime_error("cannot write BAM header");
        }
    }

    ~BamWriter() {
        if (rec_ != nullptr) {
            bam_destroy1(rec_);
        }
        if (hdr_ != nullptr) {
            sam_hdr_destroy(hdr_);
        }
        if (fp_ != nullptr) {
            hts_close(fp_);
        }
        if (!finalized_) {
            std::error_code ec;
            std::filesystem::remove(temp_path_, ec);
        }
    }

    // One nominal record: mapped (flag 0), placed at position 1 of the primary gene's contig,
    // carrying 10x tags. xt is written only when has_xt is true; tx/gl are written only when
    // non-empty (score mode passes neither, so its BAM is unchanged: no TX, no GL).
    void write_record(const std::string& qname, int32_t gene_tid, const std::string& seq,
                      const std::string& qual, const std::string& cb, const std::string& ub,
                      const std::string& gx, const std::string& gn, const std::string& xt,
                      bool has_xt, const std::string& gd, const std::string& gl = "",
                      const std::string& tx = "") {
        const size_t l_seq = seq.size();
        const uint32_t cigar = (static_cast<uint32_t>(l_seq) << BAM_CIGAR_SHIFT) | BAM_CMATCH;
        const bool have_qual = !qual.empty() && qual.size() == l_seq;
        if (bam_set1(rec_, qname.size(), qname.c_str(), /*flag=*/0, gene_tid, /*pos=*/0,
                     /*mapq=*/255, l_seq > 0 ? 1 : 0, l_seq > 0 ? &cigar : nullptr,
                     /*mtid=*/-1, /*mpos=*/-1, /*isize=*/0, l_seq, l_seq > 0 ? seq.data() : nullptr,
                     have_qual ? qual.data() : nullptr, /*l_aux=*/0) < 0) {
            throw std::runtime_error("cannot build BAM record");
        }
        append_tag("CB", cb);
        append_tag("CR", cb);  // panCollapse carries only raw values, so raw == the CB/UB values
        append_tag("UB", ub);
        append_tag("UR", ub);
        append_tag("GX", gx);
        append_tag("GN", gn);
        // GD: orientation, ';'-separated. Score mode: one 'F'/'R' per GX gene. Ledger modes: one
        // 'F'/'R' per TX transcript (parallel to TX/GX/GL), the transcript's GENE's majority
        // orientation (a gene's transcripts are co-stranded, so every transcript of a gene repeats
        // the same value). panCollapse emits BOTH orientations (run --strand both) and records the
        // orientation here so a downstream counter owns the sense/antisense policy.
        append_tag("GD", gd);
        // TX: ledger modes only -- the read's emitted compatible transcript ids, ';'-separated,
        // sorted by transcript id. GX/GD/GL below are then one entry PER TX ENTRY, not per gene (GX
        // repeats a gene once for each of its emitted isoforms -- that redundancy is what lets a
        // downstream counter map transcript->gene and derive per-gene ambiguity without a side
        // file). Absent on the Score-mode BAM, where GX/GD stay per-gene as before.
        if (!tx.empty()) {
            append_tag("TX", tx);
        }
        // GL: ledger modes only, ';'-separated and parallel to TX/GX/GD -- one 'S' (spliced: the
        // read is exon-only on this transcript, i.e. explained without touching any of its introns)
        // or 'U' (unspliced: the transcript's genomic span brackets the read but its exon path does
        // not explain it -- the read sits in one of its introns) per emitted transcript. A GENE
        // flagged both S (via one transcript) and U (via another) is velocyto's 'ambiguous' --
        // panCollapse never computes that; a downstream counter groups TX by GX and derives it, then
        // applies the count-mode rule (Gene = spliced-only, GeneFull = any) and the sense/antisense
        // policy. Absent on the Score-mode BAM, so a GL-less BAM counts by XT/GX as before.
        if (!gl.empty()) {
            append_tag("GL", gl);
        }
        if (has_xt) {
            append_tag("XT", xt);
        }
        if (sam_write1(fp_, hdr_, rec_) < 0) {
            throw std::runtime_error("cannot write BAM record");
        }
        ++record_count_;
    }

    uint64_t record_count() const { return record_count_; }

    void finalize() {
        if (hts_close(fp_) < 0) {
            fp_ = nullptr;
            throw std::runtime_error("cannot finalize BAM file");
        }
        fp_ = nullptr;
        std::filesystem::rename(temp_path_, final_path_);
        finalized_ = true;
    }

private:
    void append_tag(const char tag[2], const std::string& value) {
        if (bam_aux_append(rec_, tag, 'Z', static_cast<int>(value.size()) + 1,
                           reinterpret_cast<const uint8_t*>(value.c_str())) < 0) {
            throw std::runtime_error("cannot append BAM tag");
        }
    }

    std::filesystem::path final_path_;
    std::filesystem::path temp_path_;
    htsFile* fp_ = nullptr;
    sam_hdr_t* hdr_ = nullptr;
    bam1_t* rec_ = nullptr;
    uint64_t record_count_ = 0;
    bool finalized_ = false;
};

void write_text_file(const std::filesystem::path& filename, const std::string& contents) {
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("cannot write " + filename.string());
    }
    out << contents;
}

// D061 (splice-junction concordance): every consecutive aligned node pair a read group's own
// alignments make -- within one subpath's mapping list, or across a subpath next/connection link to
// another subpath -- is a candidate splice edge, checked by the caller against the global
// SpliceEdgeMap (pathtally::splice_concordant_transcripts). `next` links are already contiguous in
// the graph (MultipathAlignment's own contract); `connection` links are not necessarily so -- but
// both are scanned the SAME way, as plain node-id adjacency, since in this graph a splice is an
// ordinary graph edge/node-skip, not something inferred from which link type carried it. A subpath
// with zero mappings (never expected in practice, but not guaranteed by the proto) contributes no
// first/last node and is simply skipped.
std::vector<std::pair<int64_t, int64_t>> collect_read_node_pairs(
    const std::vector<vg::MultipathAlignment>& records) {
    std::vector<std::pair<int64_t, int64_t>> pairs;
    for (const vg::MultipathAlignment& record : records) {
        for (int s = 0; s < record.subpath_size(); ++s) {
            const vg::Subpath& subpath = record.subpath(s);
            const vg::Path& path = subpath.path();
            const int n = path.mapping_size();
            for (int i = 0; i + 1 < n; ++i) {
                pairs.emplace_back(path.mapping(i).position().node_id(),
                                   path.mapping(i + 1).position().node_id());
            }
            if (n == 0) {
                continue;  // no last node -- any next/connection out of this subpath contributes nothing
            }
            const int64_t last = path.mapping(n - 1).position().node_id();
            auto pair_with_first_node_of = [&](int next_index) {
                const vg::Path& next_path = record.subpath(next_index).path();
                if (next_path.mapping_size() > 0) {
                    pairs.emplace_back(last, next_path.mapping(0).position().node_id());
                }
            };
            for (const uint32_t next_index : subpath.next()) {
                pair_with_first_node_of(static_cast<int>(next_index));
            }
            for (const vg::Connection& connection : subpath.connection()) {
                pair_with_first_node_of(static_cast<int>(connection.next()));
            }
        }
    }
    return pairs;
}

struct Group {
    std::string name;
    MoleculeId molecule;
    bool skip_for_molecule_identity = false;
    MoleculeParseStatus molecule_status = MoleculeParseStatus::Ok;
    std::string molecule_message;
    std::vector<vg::MultipathAlignment> records;
    bool saw_subpath_record = false;
    bool saw_unaligned_record = false;
};

int run_convert(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    // Gene-calling source. Both score mode (D048) and the ledger count modes (D060) read the exon
    // (HST) layer the same way -- read_t2g collapses each embedded HST path's haplotype copies
    // (transcript_id_of) to a canonical transcript id, giving the RAD target dictionary
    // (transcripts) and the transcript->gene map. The ledger modes additionally read the gene-body
    // layer (--body-t2g) as a raw path->gene map -- a gene body is not itself a transcript, so it
    // is not collapsed the same way -- used for the gene-body score and the transcript_spans
    // precompute below.
    T2gData t2g = read_t2g(options.t2g);
    std::map<std::string, std::string> body_path_gene;
    if (options.count_mode != pathtally::CountMode::Score) {
        body_path_gene = read_path_gene(options.body_t2g);
    }

    xg::XG graph;
    std::ifstream xg_in(options.xg, std::ios::binary);
    if (!xg_in) {
        throw std::runtime_error("cannot open XG");
    }
    graph.deserialize(xg_in);

    // Score-mode HST lookup (node -> HST names, cached per node). Ledger-mode lookup is below.
    // Only the one for the active mode is built. Both do the node-id-space validation on first
    // touch.
    std::unordered_map<uint64_t, std::string> hst_path_name;
    std::unordered_map<int64_t, std::vector<std::pair<const std::string*, bool>>> node_hst_cache;
    pathtally::PathLookup lookup;

    // Ledger-mode state (D060: per-transcript intron-touch classification). ledger_path_info covers
    // every reference path relevant to a ledger count mode -- both exon-layer transcripts and
    // gene-body paths -- keyed by path handle, so the per-node cache below classifies a node's
    // crossing paths in O(1). exon_name_transcript / body_name_gene are the SAME classification
    // keyed by raw path NAME instead: after tally_read_group_into tallies a read's touched
    // references by name (exactly as score mode does), these collapse that tally down to
    // exon_score (per transcript) and body_score (per gene) -- see the ledger branch of flush_group.
    struct PathInfo {
        std::string name;       // raw graph path name, fed to the score tally's PathLookup
        uint32_t gene_idx = 0;   // gene index (t2g.gene_ids space)
        bool is_exon = false;    // exon-transcript path (true) vs gene-body path (false)
    };
    std::unordered_map<uint64_t, PathInfo> ledger_path_info;
    std::unordered_map<std::string, uint32_t> exon_name_transcript;  // exon path name -> transcript target id
    std::unordered_map<std::string, uint32_t> body_name_gene;         // body path name -> gene idx
    // Per-transcript spans for the spliced/unspliced classification, built once at load. For each
    // gene, each exon transcript contributes (first_on_body_exon_node_id, last_on_body_exon_node_id,
    // transcript_target_id). At read time a transcript "spans" the read iff its [lo,hi] node-id
    // interval brackets the read's gene-body node-id range; the gene body's nodes run in ascending
    // id order within the gene's contiguous id band, so a node-id interval is the span. See
    // pathtally_ledger.hpp (classify_ledger_group) for how this feeds the classification.
    std::unordered_map<uint32_t, std::vector<pathtally::TranscriptSpan>> transcript_spans;
    // D061 (splice-junction concordance): global undirected splice-edge (min,max on-body node id) ->
    // owning transcript target ids, built in the SAME precompute pass as transcript_spans below (it
    // reuses that loop's per-gene body_nodes). A read's own crossed splice edges (collect_read_node_pairs
    // below, checked via pathtally::splice_concordant_transcripts) gate the S call in
    // classify_ledger_group: a transcript explains a read's splice exon-only only if it owns every
    // edge the read's own alignment skips over -- not merely because its exon path also happens to
    // cross the same two nodes (the old, too-loose node-membership test). See pathtally_ledger.hpp.
    pathtally::SpliceEdgeMap splice_edges;
    // Per-node ledger cache. ref_paths feeds the score tally (tally_read_group_into, reused verbatim
    // from score mode): every reference path crossing the node, exon or gene-body, un-collapsed, so
    // a read's per-reference score here is exactly what score mode would compute for that
    // reference. orient is (gene_idx, path_is_reverse) collapsed per gene at this node (a node on
    // both a gene's exon and body paths keeps whichever is visited last, as before) -- used for the
    // gene's majority-orientation GD tag, from EVERY touched node (exon or body), so even a
    // purely-intronic read gets an orientation. body_genes is the deduped gene-index list whose BODY
    // path crosses this node, used only to grow that gene's touched node-id range: a gene's body may
    // be embedded as multiple haplotype copies (H1/R1) crossing the same node, and this must count
    // once per gene, not once per copy.
    struct NodeLedger {
        std::vector<std::pair<const std::string*, bool>> ref_paths;
        std::vector<std::pair<uint32_t, bool>> orient;
        std::vector<uint32_t> body_genes;
    };
    std::unordered_map<int64_t, NodeLedger> node_ledger_cache;
    std::function<const NodeLedger&(int64_t)> node_ledger_of;
    pathtally::PathLookup ledger_lookup;

    if (options.count_mode == pathtally::CountMode::Score) {
        graph.for_each_path_handle([&](const handlegraph::path_handle_t& path) {
            std::string name = graph.get_path_name(path);
            if (t2g.hst_names.find(name) != t2g.hst_names.end()) {
                hst_path_name.emplace(handlegraph::as_integer(path), std::move(name));
            }
        });
        lookup = [&](int64_t node_id, const std::function<void(const std::string&, bool)>& emit) {
            auto cached = node_hst_cache.find(node_id);
            if (cached == node_hst_cache.end()) {
                if (!graph.has_node(node_id)) {
                    throw std::runtime_error(
                        "GAMP/xg node-id-space mismatch: node " + std::to_string(node_id) +
                        " is absent from the graph; the GAMP was likely aligned to a different graph");
                }
                std::vector<std::pair<const std::string*, bool>> entries;
                const handlegraph::handle_t handle = graph.get_handle(node_id, false);
                graph.for_each_step_on_handle(handle, [&](const handlegraph::step_handle_t& step) {
                    const auto it =
                        hst_path_name.find(handlegraph::as_integer(graph.get_path_handle_of_step(step)));
                    if (it != hst_path_name.end()) {
                        entries.emplace_back(&it->second,
                                             graph.get_is_reverse(graph.get_handle_of_step(step)));
                    }
                    return true;
                });
                cached = node_hst_cache.emplace(node_id, std::move(entries)).first;
            }
            for (const auto& [name_ptr, path_is_reverse] : cached->second) {
                emit(*name_ptr, path_is_reverse);
            }
        };
    } else {
        // Gene -> its exon-transcript and gene-body path handles, collected to build transcript_spans.
        std::unordered_map<uint32_t, std::vector<handlegraph::path_handle_t>> gene_exon_paths;
        std::unordered_map<uint32_t, std::vector<handlegraph::path_handle_t>> gene_body_paths;
        // t2g.gene_ids (built by read_t2g from the EXON layer only) may not cover every gene the
        // gene-body layer names -- a gene with body coverage but no annotated exon transcript at all
        // still needs a valid index here (it contributes no transcript output -- classify_ledger_group
        // silently excludes a gene absent from transcript_spans -- but must not abort the whole load).
        // Get-or-create rather than .at(), so such a gene is assigned an index on first sight.
        auto gene_index_for = [&](const std::string& gene) -> uint32_t {
            const auto it = t2g.gene_ids.find(gene);
            if (it != t2g.gene_ids.end()) {
                return static_cast<uint32_t>(it->second);
            }
            const int32_t idx = static_cast<int32_t>(t2g.gene_names.size());
            t2g.gene_ids.emplace(gene, idx);
            t2g.gene_names.push_back(gene);
            return static_cast<uint32_t>(idx);
        };
        graph.for_each_path_handle([&](const handlegraph::path_handle_t& path) {
            const std::string name = graph.get_path_name(path);
            const std::string transcript = pathtally::transcript_id_of(name);
            // Exon layer: the SAME transcript dictionary as score mode (t2g.transcript_gene), keyed
            // by the canonical (haplotype-copy-collapsed) transcript id. transcript_gene is keyed by
            // transcript_id_of(t2g file's column 1), so this matches whether the t2g file used the
            // full embedded path name or the bare/suffix-stripped id (bare-t2g regression guard).
            const auto tg = t2g.transcript_gene.find(transcript);
            if (tg != t2g.transcript_gene.end()) {
                const uint32_t gene_idx = gene_index_for(tg->second);
                const uint32_t target_id = t2g.target_ids.at(transcript);
                ledger_path_info.emplace(handlegraph::as_integer(path), PathInfo{name, gene_idx, true});
                exon_name_transcript.emplace(name, target_id);
                gene_exon_paths[gene_idx].push_back(path);
                return;
            }
            // Gene-body layer: a raw path->gene map -- a gene body is not a transcript, so it is not
            // collapsed via transcript_id_of. Accept either the full embedded path name or the
            // suffix-stripped bare gene id, the same fallback as the exon layer above.
            auto b = body_path_gene.find(name);
            if (b == body_path_gene.end() && transcript != name) {
                b = body_path_gene.find(transcript);
            }
            if (b != body_path_gene.end()) {
                const uint32_t gene_idx = gene_index_for(b->second);
                ledger_path_info.emplace(handlegraph::as_integer(path), PathInfo{name, gene_idx, false});
                body_name_gene.emplace(name, gene_idx);
                gene_body_paths[gene_idx].push_back(path);
            }
        });

        // Per-transcript spans for the ledger classification. For each gene, gather its gene-body node
        // set, then record every exon transcript's [first,last on-body exon node id] interval. A
        // transcript's introns are the body nodes inside that interval its own exon path skips; rather
        // than materialise those (memory: hundreds of introns x tens of thousands of transcripts), the
        // read-time classification counts, per gene, transcripts that span the read vs transcripts
        // whose own exon path explains it, and infers intron touch from the difference (see
        // pathtally_ledger.hpp). Off-body exon nodes (divergent haplotype isoforms whose exons leave
        // the deduped body backbone) are excluded from the span so it stays within the gene's id band.
        // This is the one added load-time pass over the graph. D061 extends the SAME pass: while
        // walking each exon path's node sequence, also record which consecutive pairs are that
        // transcript's OWN splice (intron-skip) edges, into the global splice_edges ownership map.
        for (const auto& [gene_idx, exon_paths] : gene_exon_paths) {
            std::unordered_set<int64_t> body_nodes;
            const auto body_it = gene_body_paths.find(gene_idx);
            if (body_it != gene_body_paths.end()) {
                for (const handlegraph::path_handle_t& body_path : body_it->second) {
                    graph.for_each_step_in_path(body_path, [&](const handlegraph::step_handle_t& step) {
                        body_nodes.insert(graph.get_id(graph.get_handle_of_step(step)));
                    });
                }
            }
            // Sorted once per gene so the D061 splice-edge check below ("is there a body node
            // strictly between these two exon-path neighbours") is a binary search rather than an
            // O(range) scan -- a gene's body can span thousands of nodes.
            std::vector<int64_t> body_sorted(body_nodes.begin(), body_nodes.end());
            std::sort(body_sorted.begin(), body_sorted.end());

            auto& spans = transcript_spans[gene_idx];
            spans.reserve(exon_paths.size());
            for (const handlegraph::path_handle_t& exon_path : exon_paths) {
                std::vector<int64_t> node_ids;
                graph.for_each_step_in_path(exon_path, [&](const handlegraph::step_handle_t& step) {
                    node_ids.push_back(graph.get_id(graph.get_handle_of_step(step)));
                });
                const uint32_t target_id = exon_name_transcript.at(graph.get_path_name(exon_path));

                int64_t lo = std::numeric_limits<int64_t>::max();
                int64_t hi = std::numeric_limits<int64_t>::min();
                for (const int64_t id : node_ids) {
                    if (body_nodes.count(id) != 0) {
                        lo = std::min(lo, id);
                        hi = std::max(hi, id);
                    }
                }
                if (lo <= hi) {
                    spans.push_back({lo, hi, target_id});
                }

                // D061: a consecutive pair on this exon path is one of the transcript's own splice
                // (intron-skip) edges iff the gene body has a node strictly between them -- the
                // transcript's own path jumps past real gene-body sequence there. Record it, owned
                // by this transcript, in the global map (an edge can be several transcripts' shared
                // intron -- e.g. a constitutive junction -- so this only ever adds an owner).
                for (size_t i = 0; i + 1 < node_ids.size(); ++i) {
                    const int64_t edge_lo = std::min(node_ids[i], node_ids[i + 1]);
                    const int64_t edge_hi = std::max(node_ids[i], node_ids[i + 1]);
                    const auto between = std::upper_bound(body_sorted.begin(), body_sorted.end(), edge_lo);
                    if (between != body_sorted.end() && *between < edge_hi) {
                        splice_edges[{edge_lo, edge_hi}].insert(target_id);
                    }
                }
            }
        }

        node_ledger_of = [&](int64_t node_id) -> const NodeLedger& {
            auto cached = node_ledger_cache.find(node_id);
            if (cached == node_ledger_cache.end()) {
                if (!graph.has_node(node_id)) {
                    throw std::runtime_error(
                        "GAMP/xg node-id-space mismatch: node " + std::to_string(node_id) +
                        " is absent from the graph; the GAMP was likely aligned to a different graph");
                }
                NodeLedger nl;
                std::map<uint32_t, bool> orient_here;  // gene_idx -> is_reverse, deduped per node
                std::set<uint32_t> body_genes_here;
                const handlegraph::handle_t handle = graph.get_handle(node_id, false);
                graph.for_each_step_on_handle(handle, [&](const handlegraph::step_handle_t& step) {
                    const uint64_t path_handle =
                        handlegraph::as_integer(graph.get_path_handle_of_step(step));
                    const auto it = ledger_path_info.find(path_handle);
                    if (it == ledger_path_info.end()) {
                        return true;
                    }
                    const bool path_is_reverse = graph.get_is_reverse(graph.get_handle_of_step(step));
                    // Every reference path crossing the node (exon transcript or gene body),
                    // un-collapsed, is a scoreable reference for tally_read_group_into.
                    nl.ref_paths.emplace_back(&it->second.name, path_is_reverse);
                    orient_here[it->second.gene_idx] = path_is_reverse;
                    if (!it->second.is_exon) {
                        body_genes_here.insert(it->second.gene_idx);
                    }
                    return true;
                });
                for (const auto& [gene_idx, is_reverse] : orient_here) {
                    nl.orient.emplace_back(gene_idx, is_reverse);
                }
                nl.body_genes.assign(body_genes_here.begin(), body_genes_here.end());
                cached = node_ledger_cache.emplace(node_id, std::move(nl)).first;
            }
            return cached->second;
        };
        ledger_lookup = [&](int64_t node_id, const std::function<void(const std::string&, bool)>& emit) {
            for (const auto& [name_ptr, path_is_reverse] : node_ledger_of(node_id).ref_paths) {
                emit(*name_ptr, path_is_reverse);
            }
        };
    }

    std::shared_ptr<pathtally::QualAdjScorer> qual_adj_scorer;
    pathtally::NodeScorer node_scorer = pathtally::flat_scorer();
    if (options.score_mode == ScoreMode::QualAdj) {
        qual_adj_scorer = std::make_shared<pathtally::QualAdjScorer>();
        node_scorer = [qual_adj_scorer, &graph](const vg::MultipathAlignment& mp, int subpath_index,
                                                size_t read_offset, std::vector<int64_t>& per_node) {
            const std::string& seq = mp.sequence();
            const std::string& qual = mp.quality();
            if (!seq.empty() && qual.size() == seq.size()) {
                qual_adj_scorer->score_subpath(graph, mp.subpath(subpath_index), seq, qual, read_offset, false,
                                               &per_node);
            } else {
                pathtally::score_subpath(mp.subpath(subpath_index), read_offset, seq.size(), false,
                                         pathtally::ScoreParams{}, &per_node);
            }
        };
    }

    std::filesystem::create_directories(options.out_dir);
    uint64_t max_chunk_bytes = static_cast<uint64_t>(1) << 30;  // 1 GiB, well under the u32 chunk field
    if (const char* env = std::getenv("PANCOLLAPSE_MAX_CHUNK_BYTES")) {
        max_chunk_bytes = std::strtoull(env, nullptr, 10);
        if (max_chunk_bytes == 0) {
            max_chunk_bytes = 1;
        }
    }
    RadStreamWriter rad_writer(options.out_dir / "map.rad", t2g.target_names, options.raw_cb_length,
                               options.raw_umi_length, max_chunk_bytes);

    std::unique_ptr<BamWriter> bam_writer;
    if (!options.bam_out.empty()) {
        std::string command_line;
        for (int i = 0; i < argc; ++i) {
            if (i > 0) {
                command_line += ' ';
            }
            command_line += argv[i];
        }
        bam_writer = std::make_unique<BamWriter>(options.bam_out, t2g.gene_names, PANCOLLAPSE_VERSION,
                                                 command_line);
    }

    std::ifstream gamp_file;
    std::istream* gamp_in = &std::cin;
    if (options.gamp != std::filesystem::path("-")) {
        gamp_file.open(options.gamp, std::ios::binary);
        if (!gamp_file) {
            throw std::runtime_error("cannot open GAMP");
        }
        gamp_in = &gamp_file;
    }

    size_t input_records = 0;
    size_t input_read_groups = 0;
    size_t grouping_recurrence_failures = 0;
    size_t no_compatible_transcript_groups = 0;
    size_t strand_filtered_groups = 0;
    size_t multigene_dropped_groups = 0;
    size_t unaligned_reads = 0;
    size_t raw_molecule_missing_groups = 0;
    size_t raw_molecule_malformed_groups = 0;
    size_t raw_molecule_unsupported_groups = 0;
    size_t raw_molecule_skipped_groups = 0;
    // Histogram of emitted-group target-set sizes: target_count -> group_count.
    std::map<size_t, size_t> emitted_target_histogram;
    // completed_names: one entry per read-group name that has been fully processed and
    // flushed. Used exclusively to detect non-contiguous (unsorted) GAMP input: if a
    // completed name recurs the run aborts rather than silently splitting the read's
    // alignments into separate groups. Peak resident memory is O(number of distinct
    // read-group names seen); on a whole-genome run this can reach many GB. Bounding
    // or removing the set would trade that memory for a silent-correctness risk — a
    // read whose alignments span non-adjacent GAMP positions would be mis-grouped
    // without detection — so the full set is kept as the deliberate memory/correctness
    // tradeoff for v0.1.
    std::set<std::string> completed_names;
    bool have_group = false;
    Group current_group;
    // Reused across every group so the per-group tally does not reallocate its table.
    pathtally::TallyMap tally_workspace;

    auto start_group = [&](const vg::MultipathAlignment& alignment) {
        if (completed_names.count(alignment.name()) != 0) {
            ++grouping_recurrence_failures;
            throw std::runtime_error("grouping_recurrence_failures: completed GAMP read name recurred");
        }
        current_group = {};
        current_group.name = alignment.name();
        ++input_read_groups;
        const MoleculeParseResult molecule =
            parse_molecule_id(alignment.name(), options.raw_cb_length, options.raw_umi_length);
        if (molecule.status == MoleculeParseStatus::Ok) {
            current_group.molecule = molecule.id;
        } else {
            current_group.skip_for_molecule_identity = true;
            current_group.molecule_status = molecule.status;
            current_group.molecule_message = molecule.message;
            if (options.molecule_identity_failures == MoleculeIdentityFailurePolicy::Fail) {
                throw std::runtime_error(molecule_status_counter(molecule.status) + "=1: " + molecule.message);
            }
        }
        have_group = true;
    };

    auto flush_group = [&]() {
        if (!have_group) {
            return;
        }
        if (current_group.skip_for_molecule_identity) {
            ++raw_molecule_skipped_groups;
            switch (current_group.molecule_status) {
            case MoleculeParseStatus::Missing:
                ++raw_molecule_missing_groups;
                break;
            case MoleculeParseStatus::Malformed:
                ++raw_molecule_malformed_groups;
                break;
            case MoleculeParseStatus::Unsupported:
                ++raw_molecule_unsupported_groups;
                break;
            case MoleculeParseStatus::Ok:
                throw std::runtime_error("internal molecule identity skip state mismatch");
            }
            std::cerr << "panCollapse: warning: skipped read group with invalid raw molecule identity: "
                      << current_group.name << ": " << current_group.molecule_message << '\n';
            completed_names.insert(current_group.name);
            have_group = false;
            return;
        }

        std::vector<pathtally::RadTarget> targets;
        // Distinct genes among a ledger read's emitted transcripts (score mode leaves this empty --
        // it has no Unique multi-GENE drop; a score-mode multi-target read is ordinary D048
        // multimapping evidence). Populated inside the ledger branch below; read after it, for the
        // Unique RAD drop, which is a multi-GENE rule, not a multi-transcript one (a gene with >=2
        // emitted isoforms is still one gene).
        std::set<std::string> ledger_genes;
        if (options.count_mode == pathtally::CountMode::Score) {
            std::vector<const vg::MultipathAlignment*> record_ptrs;
            record_ptrs.reserve(current_group.records.size());
            for (const vg::MultipathAlignment& record : current_group.records) {
                record_ptrs.push_back(&record);
            }
            pathtally::tally_read_group_into(tally_workspace, record_ptrs, lookup, node_scorer);
            targets = pathtally::select_targets(tally_workspace);
        } else {
            // Ledger modes (D060): reuse the score-mode tally verbatim -- tally_read_group_into over
            // a PathLookup covering BOTH exon-layer transcripts and gene-body paths (ledger_lookup) --
            // to score every reference the read touches, then classify per transcript on top of it.
            std::vector<const vg::MultipathAlignment*> record_ptrs;
            record_ptrs.reserve(current_group.records.size());
            for (const vg::MultipathAlignment& record : current_group.records) {
                record_ptrs.push_back(&record);
            }
            pathtally::tally_read_group_into(tally_workspace, record_ptrs, ledger_lookup, node_scorer);

            // A second, plain node visitation (score-independent) for what the score tally cannot
            // give us: per-gene orientation (aligned bases, majority vote -- same rule as score
            // mode's RadTarget.forward) and the read's touched gene-body node-id range per gene, used
            // to test each transcript's precomputed span below.
            std::map<uint32_t, std::pair<int64_t, int64_t>> gene_orient;  // gene idx -> (fwd, rev) bases
            std::map<uint32_t, std::pair<int64_t, int64_t>> body_range;    // gene idx -> (lo, hi) touched
            for (const vg::MultipathAlignment& record : current_group.records) {
                for (int s = 0; s < record.subpath_size(); ++s) {
                    const vg::Path& path = record.subpath(s).path();
                    for (int i = 0; i < path.mapping_size(); ++i) {
                        const vg::Mapping& mapping = path.mapping(i);
                        const int64_t aligned = pathtally::mapping_aligned_bases(mapping);
                        if (aligned == 0) {
                            continue;
                        }
                        const int64_t node_id = mapping.position().node_id();
                        const bool read_is_reverse = mapping.position().is_reverse();
                        const NodeLedger& nl = node_ledger_of(node_id);
                        for (const auto& [gene_idx, path_is_reverse] : nl.orient) {
                            auto& orient = gene_orient[gene_idx];
                            if (read_is_reverse == path_is_reverse) {
                                orient.first += aligned;
                            } else {
                                orient.second += aligned;
                            }
                        }
                        for (const uint32_t gene_idx : nl.body_genes) {
                            auto [it, inserted] = body_range.try_emplace(
                                gene_idx, std::numeric_limits<int64_t>::max(),
                                std::numeric_limits<int64_t>::min());
                            it->second.first = std::min(it->second.first, node_id);
                            it->second.second = std::max(it->second.second, node_id);
                        }
                    }
                }
            }

            // Collapse the by-name score tally to exon_score (per transcript target id) and
            // body_score (per gene idx), each the MAX score among that transcript's/gene's raw
            // haplotype-copy paths -- not a sum, so a node shared by two haplotype copies of the
            // same transcript or gene body is not double-counted (the best-explaining copy's own
            // score already reflects every node on ITS path once).
            std::map<uint32_t, int64_t> exon_score;
            std::map<uint32_t, int64_t> body_score;
            for (const auto& [name, tally] : tally_workspace) {
                const auto ex = exon_name_transcript.find(name);
                if (ex != exon_name_transcript.end()) {
                    auto [it, inserted] = exon_score.try_emplace(ex->second, std::numeric_limits<int64_t>::min());
                    it->second = std::max(it->second, tally.score);
                    continue;
                }
                const auto bd = body_name_gene.find(name);
                if (bd != body_name_gene.end()) {
                    auto [it, inserted] = body_score.try_emplace(bd->second, std::numeric_limits<int64_t>::min());
                    it->second = std::max(it->second, tally.score);
                }
            }

            // D061: the read group's own splice-concordant transcript set -- nullopt (unconstrained)
            // unless its alignments actually cross a splice edge (collect_read_node_pairs), in which
            // case only a transcript owning EVERY crossed edge stays eligible for S below.
            const std::optional<std::set<uint32_t>> splice_concordant =
                pathtally::splice_concordant_transcripts(collect_read_node_pairs(current_group.records),
                                                          splice_edges);

            // classify_ledger_group (pathtally_ledger.hpp): spliced = a transcript whose own exon-path
            // score ties the read's top (within kIntronFlankBases) AND is splice-concordant (D061);
            // unspliced = a transcript that spans the read's touched gene-body range, for a gene whose
            // body ties top, and is not already spliced. Emitted once per transcript, deduped.
            const std::vector<pathtally::LedgerCall> calls =
                pathtally::classify_ledger_group(exon_score, body_score, body_range, transcript_spans,
                                                 kIntronFlankBases, splice_concordant);

            // Per emitted transcript: its gene and the gene's majority orientation (a gene's
            // transcripts are co-stranded, so every transcript of a gene carries the SAME GD).
            // ledger_genes collects the distinct genes among the emitted transcripts, for the
            // Unique multi-GENE (not multi-transcript) RAD drop below.
            std::map<uint32_t, char> transcript_gd;  // transcript target id -> 'F'/'R'
            for (const pathtally::LedgerCall& call : calls) {
                const std::string& transcript = t2g.target_names[call.transcript_target_id];
                const std::string& gene = t2g.transcript_gene.at(transcript);
                ledger_genes.insert(gene);
                const uint32_t gene_idx = static_cast<uint32_t>(t2g.gene_ids.at(gene));
                const auto oit = gene_orient.find(gene_idx);
                const bool forward = oit == gene_orient.end() || oit->second.first >= oit->second.second;
                transcript_gd[call.transcript_target_id] = forward ? 'F' : 'R';
            }

            // Emit the per-transcript ledger into the BAM: TX = emitted transcript ids, GX/GD/GL one
            // entry per TX entry (parallel) -- gene, orientation, spliced('S')/unspliced('U'). GX
            // repeats a gene once per one of its emitted isoforms; that redundancy is what lets
            // count_cr map transcript->gene and derive ambiguity without a side file. The count mode,
            // ambiguity resolution, and strand policy all live in count_cr -- panCollapse classifies
            // per transcript, the counter groups by gene and collapses. A single-gene read is always
            // written (XT set). A multi-gene read follows --bam-multigene, mirroring the RAD Unique
            // drop below: omit leaves it out of the BAM, first assigns it to the primary gene (XT
            // set), all carries it with the full candidate set and no XT so count_cr's
            // MultiGeneUMI_CR rescue can resolve the dominant gene.
            if (bam_writer && !calls.empty()) {
                std::string tx, gx, gd, gl, primary_gene;
                int32_t primary_tid = -1;
                size_t n = 0;
                for (const pathtally::LedgerCall& call : calls) {
                    const std::string& transcript = t2g.target_names[call.transcript_target_id];
                    const std::string& gene = t2g.transcript_gene.at(transcript);
                    if (n > 0) {
                        tx += ';';
                        gx += ';';
                        gd += ';';
                        gl += ';';
                    }
                    tx += transcript;
                    gx += gene;
                    gd += transcript_gd.at(call.transcript_target_id);
                    gl += call.spliced ? 'S' : 'U';
                    if (n == 0) {
                        primary_gene = gene;
                        primary_tid = t2g.gene_ids.at(gene);
                    }
                    ++n;
                }
                const bool multigene = ledger_genes.size() > 1;
                if (!multigene || options.bam_multigene != BamMultiGenePolicy::Omit) {
                    const bool has_xt =
                        !multigene || options.bam_multigene == BamMultiGenePolicy::First;
                    const vg::MultipathAlignment& read = current_group.records.front();
                    bam_writer->write_record(current_group.molecule.original_name, primary_tid,
                                             read.sequence(), read.quality(),
                                             current_group.molecule.barcode, current_group.molecule.umi,
                                             gx, /*gn=*/gx, /*xt=*/primary_gene, /*has_xt=*/has_xt,
                                             /*gd=*/gd, /*gl=*/gl, /*tx=*/tx);
                }
            }

            // RAD/alevin-fry: the emitted transcripts are the read's equivalence class.
            for (const pathtally::LedgerCall& call : calls) {
                targets.push_back({t2g.target_names[call.transcript_target_id],
                                   transcript_gd.at(call.transcript_target_id) == 'F'});
            }
        }

        if (targets.empty()) {
            ++no_compatible_transcript_groups;
            if (current_group.saw_unaligned_record && !current_group.saw_subpath_record) {
                ++unaligned_reads;
            }
            completed_names.insert(current_group.name);
            have_group = false;
            return;
        }

        // Optional target-relative orientation filter: keep only sense (forward) or antisense
        // (reverse) targets. A read that had compatible targets but none in the wanted
        // orientation emits no record and is counted separately from no-compatible.
        if (options.strand != StrandFilter::Both) {
            const bool keep_forward = options.strand == StrandFilter::Forward;
            targets.erase(std::remove_if(targets.begin(), targets.end(),
                                         [keep_forward](const pathtally::RadTarget& target) {
                                             return target.forward != keep_forward;
                                         }),
                          targets.end());
            if (targets.empty()) {
                ++strand_filtered_groups;
                completed_names.insert(current_group.name);
                have_group = false;
                return;
            }
        }

        // Ledger count modes are Unique (CellRanger default) for the RAD/alevin-fry path: a read
        // compatible with more than one GENE gets no RAD record (a gene with >=2 emitted transcript
        // isoforms -- e.g. the cross-isoform spliced+unspliced case -- is still one gene and is NOT
        // dropped here). The optional BAM already emitted the read above under --bam-multigene (all
        // carries the multi-gene read with its full candidate set for count_cr's downstream
        // MultiGeneUMI_CR rescue; omit/first do not), independent of this RAD drop, so one alignment
        // serves both the Unique RAD and the mode-agnostic BAM.
        const bool ledger_multigene =
            options.count_mode != pathtally::CountMode::Score && ledger_genes.size() > 1;
        if (ledger_multigene) {
            ++multigene_dropped_groups;
            completed_names.insert(current_group.name);
            have_group = false;
            return;
        }

        std::vector<TargetHit> hits;
        if (!ledger_multigene) {
            hits.reserve(targets.size());
            for (const pathtally::RadTarget& target : targets) {
                const auto target_id = t2g.target_ids.find(target.transcript);
                if (target_id == t2g.target_ids.end()) {
                    throw std::runtime_error("compatible transcript " + target.transcript + " is absent from the t2g");
                }
                hits.push_back({target_id->second, target.forward});
            }
            rad_writer.write_record(current_group.molecule, hits);
        }
        if (bam_writer && options.count_mode == pathtally::CountMode::Score) {
            // Score mode keeps the transcript-target BAM: collapse the read's transcript targets to
            // the sorted unique gene set. (Ledger modes emit their mode-agnostic full-ledger BAM
            // above, before the RAD Unique/strand drops.) std::set gives the deterministic order for
            // GX and for the primary (first) gene contig.
            std::set<std::string> genes;
            std::map<std::string, bool> gene_forward;  // gene -> sense iff any of its targets is forward
            for (const pathtally::RadTarget& target : targets) {
                const std::string& gene = t2g.transcript_gene.at(target.transcript);
                genes.insert(gene);
                bool& f = gene_forward[gene];
                f = f || target.forward;
            }
            std::string gx, gd;  // gd: 'F'/'R' per gene, parallel to gx (the sorted unique gene set)
            for (const std::string& gene : genes) {
                if (!gx.empty()) {
                    gx += ';';
                    gd += ';';
                }
                gx += gene;
                gd += gene_forward[gene] ? 'F' : 'R';
            }
            const std::string& primary_gene = *genes.begin();
            const bool has_xt =
                genes.size() == 1 || options.bam_multigene == BamMultiGenePolicy::First;
            const vg::MultipathAlignment& read = current_group.records.front();
            bam_writer->write_record(current_group.molecule.original_name,
                                     t2g.gene_ids.at(primary_gene), read.sequence(), read.quality(),
                                     current_group.molecule.barcode, current_group.molecule.umi, gx,
                                     /*gn=*/gx, /*xt=*/primary_gene, has_xt, /*gd=*/gd);
        }
        // hits stays empty for a ledger multi-gene read (RAD skipped even when the BAM rescues
        // it), so the RAD emitted-target histogram reflects only actual RAD records.
        if (!ledger_multigene) {
            ++emitted_target_histogram[hits.size()];
        }
        completed_names.insert(current_group.name);
        have_group = false;
    };

    vg::io::for_each<vg::MultipathAlignment>(*gamp_in, [&](vg::MultipathAlignment& alignment) {
        ++input_records;
        if (!have_group) {
            start_group(alignment);
        } else if (alignment.name() != current_group.name) {
            flush_group();
            start_group(alignment);
        }
        if (!current_group.skip_for_molecule_identity) {
            if (alignment.subpath_size() == 0) {
                current_group.saw_unaligned_record = true;
            } else {
                current_group.saw_subpath_record = true;
                // Move, not copy: for_each hands us a mutable record it re-inits before the
                // next parse, and we do not touch it again here. Copying deep-copied the whole
                // protobuf MultipathAlignment (~21% of runtime in profiling); the move steals
                // its internal storage instead.
                current_group.records.push_back(std::move(alignment));
            }
        }
    });
    flush_group();

    if (input_records == 0) {
        throw std::runtime_error("panCollapse convert expects at least one GAMP record");
    }

    rad_writer.finalize();
    if (bam_writer) {
        bam_writer->finalize();
    }
    std::string tx2gene;
    for (const std::string& target_name : t2g.target_names) {
        tx2gene += target_name + '\t' + t2g.transcript_gene.at(target_name) + '\n';
    }
    write_text_file(options.out_dir / "tx2gene.tsv", tx2gene);
    // Build the emitted-target-count histogram as "tc:gc;tc:gc;..." sorted ascending by target count.
    // Each entry is target_count:group_count.  Empty when no groups were emitted.
    std::string histogram_value;
    for (const auto& [target_count, group_count] : emitted_target_histogram) {
        if (!histogram_value.empty()) {
            histogram_value += ';';
        }
        histogram_value += std::to_string(target_count) + ':' + std::to_string(group_count);
    }
    write_text_file(options.out_dir / "summary.tsv",
                    "input_records\t" + std::to_string(input_records) + "\ninput_read_groups\t" +
                        std::to_string(input_read_groups) + "\nemitted_groups\t" +
                        std::to_string(rad_writer.record_count()) + "\nno_compatible_transcript_groups\t" +
                        std::to_string(no_compatible_transcript_groups) + "\nstrand_filtered_groups\t" +
                        std::to_string(strand_filtered_groups) + "\nmultigene_dropped_groups\t" +
                        std::to_string(multigene_dropped_groups) + "\nunaligned_reads\t" +
                        std::to_string(unaligned_reads) + "\nraw_molecule_missing_groups\t" +
                        std::to_string(raw_molecule_missing_groups) + "\nraw_molecule_malformed_groups\t" +
                        std::to_string(raw_molecule_malformed_groups) + "\nraw_molecule_unsupported_groups\t" +
                        std::to_string(raw_molecule_unsupported_groups) + "\nraw_molecule_skipped_groups\t" +
                        std::to_string(raw_molecule_skipped_groups) + "\ngrouping_recurrence_failures\t" +
                        std::to_string(grouping_recurrence_failures) + "\nemitted_target_count_histogram\t" +
                        histogram_value + "\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << kUsageText << '\n';
        return 0;
    }
    const std::string first_arg = argv[1];
    if (first_arg == "--version" || first_arg == "-V") {
        std::cout << "panCollapse " << PANCOLLAPSE_VERSION << '\n';
        return 0;
    }
    if (first_arg == "--help" || first_arg == "-h") {
        std::cout << kUsageText << '\n';
        return 0;
    }
    try {
        return run_convert(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "panCollapse: " << e.what() << '\n';
        return 1;
    }
}
