#include <handlegraph/path_position_handle_graph.hpp>
#include <handlegraph/util.hpp>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include "pathtally.hpp"
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
#include <set>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace {

constexpr uint8_t kRadU8 = 1;
constexpr uint8_t kRadU16 = 2;
constexpr uint8_t kRadU32 = 3;
constexpr uint8_t kRadU64 = 4;
constexpr uint32_t kRadForwardMask = 0x80000000U;
constexpr uint32_t kRadTargetIdMask = 0x7fffffffU;

enum class MoleculeIdentityFailurePolicy { Skip, Fail };

enum class ScoreMode { Flat, QualAdj };

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
};

constexpr const char* kUsageText =
    "usage: panCollapse convert --gamp reads.gamp|- --xg graph.xg --t2g t2g.tsv "
    "--out-dir out [--raw-cb-length N] [--raw-umi-length N] "
    "[--score flat|qualadj] [--molecule-identity-failures skip|fail]";

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
    }
    return data;
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
class RadStreamWriter {
public:
    RadStreamWriter(const std::filesystem::path& filename, const std::vector<std::string>& target_names,
                    size_t cb_length, size_t umi_length, uint64_t max_chunk_bytes)
        : out_(filename, std::ios::binary),
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
};

void write_text_file(const std::filesystem::path& filename, const std::string& contents) {
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("cannot write " + filename.string());
    }
    out << contents;
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
    const T2gData t2g = read_t2g(options.t2g);

    xg::XG graph;
    std::ifstream xg_in(options.xg, std::ios::binary);
    if (!xg_in) {
        throw std::runtime_error("cannot open XG");
    }
    graph.deserialize(xg_in);

    // Precompute the HST paths (those named in the t2g) as a path-handle -> name map,
    // so the per-node lookup avoids building a path-name string and hashing it for every
    // step. Reference backbones and shared exon nodes carry many steps, so this per-step
    // string work dominated the run; the precomputed integer-keyed map removes it.
    std::unordered_map<uint64_t, std::string> hst_path_name;
    graph.for_each_path_handle([&](const handlegraph::path_handle_t& path) {
        std::string name = graph.get_path_name(path);
        if (t2g.hst_names.find(name) != t2g.hst_names.end()) {
            hst_path_name.emplace(handlegraph::as_integer(path), std::move(name));
        }
    });

    // Injected HST lookup: emit each HST path crossing a node, with its orientation there.
    pathtally::PathLookup lookup = [&](int64_t node_id,
                                       const std::function<void(const std::string&, bool)>& emit) {
        if (!graph.has_node(node_id)) {
            throw std::runtime_error(
                "GAMP/xg node-id-space mismatch: node " + std::to_string(node_id) +
                " is absent from the graph; the GAMP was likely aligned to a different graph");
        }
        const handlegraph::handle_t handle = graph.get_handle(node_id, false);
        graph.for_each_step_on_handle(handle, [&](const handlegraph::step_handle_t& step) {
            const auto it = hst_path_name.find(handlegraph::as_integer(graph.get_path_handle_of_step(step)));
            if (it == hst_path_name.end()) {
                return true;
            }
            emit(it->second, graph.get_is_reverse(graph.get_handle_of_step(step)));
            return true;
        });
    };

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
    size_t unaligned_reads = 0;
    size_t raw_molecule_missing_groups = 0;
    size_t raw_molecule_malformed_groups = 0;
    size_t raw_molecule_unsupported_groups = 0;
    size_t raw_molecule_skipped_groups = 0;
    // Histogram of emitted-group target-set sizes: target_count -> group_count.
    std::map<size_t, size_t> emitted_target_histogram;
    std::set<std::string> completed_names;
    bool have_group = false;
    Group current_group;

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

        std::vector<const vg::MultipathAlignment*> record_ptrs;
        record_ptrs.reserve(current_group.records.size());
        for (const vg::MultipathAlignment& record : current_group.records) {
            record_ptrs.push_back(&record);
        }
        const std::map<std::string, pathtally::HstTally> tallies =
            pathtally::tally_read_group(record_ptrs, lookup, node_scorer);
        const std::vector<pathtally::RadTarget> targets = pathtally::select_targets(tallies);

        if (targets.empty()) {
            ++no_compatible_transcript_groups;
            if (current_group.saw_unaligned_record && !current_group.saw_subpath_record) {
                ++unaligned_reads;
            }
            completed_names.insert(current_group.name);
            have_group = false;
            return;
        }

        std::vector<TargetHit> hits;
        hits.reserve(targets.size());
        for (const pathtally::RadTarget& target : targets) {
            const auto target_id = t2g.target_ids.find(target.transcript);
            if (target_id == t2g.target_ids.end()) {
                throw std::runtime_error("compatible transcript " + target.transcript + " is absent from the t2g");
            }
            hits.push_back({target_id->second, target.forward});
        }
        rad_writer.write_record(current_group.molecule, hits);
        ++emitted_target_histogram[hits.size()];
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
                current_group.records.push_back(alignment);
            }
        }
    });
    flush_group();

    if (input_records == 0) {
        throw std::runtime_error("panCollapse convert expects at least one GAMP record");
    }

    rad_writer.finalize();
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
                        std::to_string(no_compatible_transcript_groups) + "\nunaligned_reads\t" +
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
