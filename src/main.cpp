#include <handlegraph/path_position_handle_graph.hpp>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include "pathtally.hpp"
#include "pathtally_qualadj.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
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

[[noreturn]] void usage_error() {
    throw std::runtime_error(
        "usage: panCollapse convert --gamp reads.gamp --xg graph.xg --t2g t2g.tsv "
        "--out-dir out [--raw-cb-length N] [--raw-umi-length N] "
        "[--score flat|qualadj] [--molecule-identity-failures skip|fail]");
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

void write_rad_file(const std::filesystem::path& filename,
                    const std::vector<std::string>& target_names,
                    const std::vector<EmittedRecord>& records,
                    size_t cb_length,
                    size_t umi_length) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write RAD output");
    }

    const uint8_t cb_type = rad_int_type_for_length(cb_length);
    const uint8_t umi_type = rad_int_type_for_length(umi_length);

    write_le<uint8_t>(out, 0);
    write_le<uint64_t>(out, static_cast<uint64_t>(target_names.size()));
    for (const std::string& target_name : target_names) {
        write_string_with_u16_length(out, target_name);
    }
    write_le<uint64_t>(out, records.empty() ? 0 : 1);

    write_le<uint16_t>(out, 2);
    write_tag(out, "cblen", kRadU16);
    write_tag(out, "ulen", kRadU16);

    write_le<uint16_t>(out, 2);
    write_tag(out, "b", cb_type);
    write_tag(out, "u", umi_type);

    write_le<uint16_t>(out, 1);
    write_tag(out, "compressed_ori_refid", kRadU32);

    write_le<uint16_t>(out, static_cast<uint16_t>(cb_length));
    write_le<uint16_t>(out, static_cast<uint16_t>(umi_length));

    if (records.empty()) {
        return;
    }

    const size_t read_value_size = (cb_type == kRadU64 ? 8 : cb_type == kRadU32 ? 4 : cb_type == kRadU16 ? 2 : 1) +
                                   (umi_type == kRadU64 ? 8 : umi_type == kRadU32 ? 4 : umi_type == kRadU16 ? 2 : 1);
    size_t chunk_size = 8;
    for (const EmittedRecord& record : records) {
        if (record.hits.empty()) {
            throw std::runtime_error("cannot write RAD record with no target hits");
        }
        chunk_size += 4 + read_value_size + 4 * record.hits.size();
    }
    if (records.size() > std::numeric_limits<uint32_t>::max() || chunk_size > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("RAD chunk is too large");
    }
    write_le<uint32_t>(out, static_cast<uint32_t>(chunk_size));
    write_le<uint32_t>(out, static_cast<uint32_t>(records.size()));
    for (const EmittedRecord& record : records) {
        write_le<uint32_t>(out, static_cast<uint32_t>(record.hits.size()));
        write_packed_value(out, cb_type, pack_sequence(record.molecule.barcode));
        write_packed_value(out, umi_type, pack_sequence(record.molecule.umi));
        for (const TargetHit& hit : record.hits) {
            if (hit.target_id > kRadTargetIdMask) {
                throw std::runtime_error("RAD target ID exceeds 31-bit encoding limit");
            }
            write_le<uint32_t>(out, hit.target_id | (hit.is_forward ? kRadForwardMask : 0));
        }
    }
}

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

    // Injected HST lookup: emit each HST path (present in the t2g) crossing a node,
    // with the path's orientation there.
    pathtally::PathLookup lookup = [&](int64_t node_id,
                                       const std::function<void(const std::string&, bool)>& emit) {
        if (!graph.has_node(node_id)) {
            return;
        }
        const handlegraph::handle_t handle = graph.get_handle(node_id, false);
        graph.for_each_step_on_handle(handle, [&](const handlegraph::step_handle_t& step) {
            const std::string path_name = graph.get_path_name(graph.get_path_handle_of_step(step));
            if (t2g.hst_names.find(path_name) == t2g.hst_names.end()) {
                return true;
            }
            const bool path_is_reverse = graph.get_is_reverse(graph.get_handle_of_step(step));
            emit(path_name, path_is_reverse);
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

    std::ifstream gamp_in(options.gamp, std::ios::binary);
    if (!gamp_in) {
        throw std::runtime_error("cannot open GAMP");
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
    std::vector<EmittedRecord> emitted_records;
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
        emitted_records.push_back({current_group.molecule, hits});
        completed_names.insert(current_group.name);
        have_group = false;
    };

    vg::io::for_each<vg::MultipathAlignment>(gamp_in, [&](vg::MultipathAlignment& alignment) {
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

    std::filesystem::create_directories(options.out_dir);
    write_rad_file(options.out_dir / "map.rad", t2g.target_names, emitted_records, options.raw_cb_length,
                   options.raw_umi_length);
    std::string tx2gene;
    for (const std::string& target_name : t2g.target_names) {
        tx2gene += target_name + '\t' + t2g.transcript_gene.at(target_name) + '\n';
    }
    write_text_file(options.out_dir / "tx2gene.tsv", tx2gene);
    write_text_file(options.out_dir / "summary.tsv",
                    "input_records\t" + std::to_string(input_records) + "\ninput_read_groups\t" +
                        std::to_string(input_read_groups) + "\nemitted_groups\t" +
                        std::to_string(emitted_records.size()) + "\nno_compatible_transcript_groups\t" +
                        std::to_string(no_compatible_transcript_groups) + "\nunaligned_reads\t" +
                        std::to_string(unaligned_reads) + "\nraw_molecule_missing_groups\t" +
                        std::to_string(raw_molecule_missing_groups) + "\nraw_molecule_malformed_groups\t" +
                        std::to_string(raw_molecule_malformed_groups) + "\nraw_molecule_unsupported_groups\t" +
                        std::to_string(raw_molecule_unsupported_groups) + "\nraw_molecule_skipped_groups\t" +
                        std::to_string(raw_molecule_skipped_groups) + "\ngrouping_recurrence_failures\t" +
                        std::to_string(grouping_recurrence_failures) + "\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run_convert(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "panCollapse: " << e.what() << '\n';
        return 1;
    }
}
