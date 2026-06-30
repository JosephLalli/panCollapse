#include <handlegraph/path_position_handle_graph.hpp>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr uint8_t kRadU8 = 1;
constexpr uint8_t kRadU16 = 2;
constexpr uint8_t kRadU32 = 3;
constexpr uint8_t kRadU64 = 4;
constexpr uint32_t kRadForwardMask = 0x80000000U;
constexpr uint32_t kRadTargetIdMask = 0x7fffffffU;

using SourceKey = std::pair<std::string, std::string>;

enum class MoleculeIdentityFailurePolicy {
    Skip,
    Fail,
};

enum class MoleculeParseStatus {
    Ok,
    Missing,
    Malformed,
    Unsupported,
};

struct Options {
    std::filesystem::path gamp;
    std::filesystem::path xg;
    std::filesystem::path gtf;
    std::filesystem::path manifest;
    std::filesystem::path out_dir;
    size_t raw_cb_length = 16;
    size_t raw_umi_length = 12;
    size_t score_window = 0;
    size_t min_splice_jump = 20;
    size_t max_traversals_per_read = 100000;
    MoleculeIdentityFailurePolicy molecule_identity_failures = MoleculeIdentityFailurePolicy::Skip;
};

struct ManifestRow {
    std::string source_path;
    std::string source_transcript;
    std::string canonical_transcript;
    std::string canonical_gene;
};

struct TranscriptModel {
    std::string source_path;
    std::string source_transcript;
    std::string source_gene;
    std::vector<std::pair<size_t, size_t>> exons;
    std::vector<std::pair<size_t, size_t>> introns;
    bool is_reverse = false;
};

struct ManifestData {
    std::map<SourceKey, ManifestRow> source_rows;
    std::map<std::string, std::string> target_gene;
    std::map<std::string, uint32_t> target_ids;
    std::vector<std::string> target_names;
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

struct ProjectedBlock {
    std::string source_path;
    size_t start = 0;
    size_t end = 0;
    bool is_forward_on_path = true;
    bool previous_edge_is_connection = false;
    size_t edit_index = 0;
};

struct TargetHit {
    uint32_t target_id = 0;
    bool is_forward = true;
};

struct TargetEvidence {
    int64_t best_score = std::numeric_limits<int64_t>::min();
    std::set<bool> orientations;
};

struct EmittedRecord {
    MoleculeId molecule;
    std::vector<TargetHit> hits;
};

struct ReadGroupState {
    std::string name;
    MoleculeId molecule;
    bool skip_for_molecule_identity = false;
    MoleculeParseStatus molecule_status = MoleculeParseStatus::Ok;
    std::string molecule_message;
    std::map<uint32_t, TargetEvidence> target_evidence;
    size_t traversal_count = 0;
};

struct Traversal {
    std::vector<uint32_t> subpaths;
    std::vector<bool> incoming_connection;
    int64_t score = 0;
};

struct ProjectedTraversal {
    std::vector<ProjectedBlock> blocks;
    size_t reference_edit_count = 0;
};

[[noreturn]] void usage_error() {
    throw std::runtime_error(
        "usage: panCollapse convert --gamp reads.gamp --xg graph.xg --gtf annotation.gtf "
        "--collapse-manifest collapse.tsv --out-dir out [--raw-cb-length N] [--raw-umi-length N] "
        "[--score-window N] [--min-splice-jump N] [--max-traversals-per-read N] "
        "[--molecule-identity-failures skip|fail]");
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
        } else if (arg == "--gtf") {
            options.gtf = require_value("--gtf");
        } else if (arg == "--collapse-manifest") {
            options.manifest = require_value("--collapse-manifest");
        } else if (arg == "--out-dir") {
            options.out_dir = require_value("--out-dir");
        } else if (arg == "--raw-cb-length") {
            options.raw_cb_length = parse_size_option("--raw-cb-length", require_value("--raw-cb-length"));
        } else if (arg == "--raw-umi-length") {
            options.raw_umi_length = parse_size_option("--raw-umi-length", require_value("--raw-umi-length"));
        } else if (arg == "--score-window") {
            options.score_window = parse_size_option("--score-window", require_value("--score-window"));
        } else if (arg == "--min-splice-jump") {
            options.min_splice_jump = parse_size_option("--min-splice-jump", require_value("--min-splice-jump"));
        } else if (arg == "--max-traversals-per-read") {
            options.max_traversals_per_read =
                parse_size_option("--max-traversals-per-read", require_value("--max-traversals-per-read"));
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

    if (options.gamp.empty() || options.xg.empty() || options.gtf.empty() || options.manifest.empty() ||
        options.out_dir.empty()) {
        usage_error();
    }
    if (options.raw_cb_length == 0 || options.raw_cb_length > 32 || options.raw_umi_length == 0 ||
        options.raw_umi_length > 32) {
        throw std::runtime_error("raw barcode and UMI lengths must be in 1..32");
    }
    if (options.min_splice_jump == 0 || options.max_traversals_per_read == 0) {
        throw std::runtime_error("min splice jump and max traversals per read must be at least 1");
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

std::string gtf_attribute(const std::string& attributes, const std::string& key) {
    const std::string needle = key + " \"";
    const size_t start = attributes.find(needle);
    if (start == std::string::npos) {
        return {};
    }
    const size_t value_start = start + needle.size();
    const size_t value_end = attributes.find('"', value_start);
    if (value_end == std::string::npos) {
        return {};
    }
    return attributes.substr(value_start, value_end - value_start);
}

std::vector<std::pair<size_t, size_t>> merge_intervals(std::vector<std::pair<size_t, size_t>> intervals) {
    if (intervals.empty()) {
        return intervals;
    }
    std::sort(intervals.begin(), intervals.end());
    std::vector<std::pair<size_t, size_t>> merged;
    merged.push_back(intervals.front());
    for (size_t i = 1; i < intervals.size(); ++i) {
        auto& back = merged.back();
        if (intervals[i].first <= back.second) {
            back.second = std::max(back.second, intervals[i].second);
        } else {
            merged.push_back(intervals[i]);
        }
    }
    return merged;
}

std::vector<std::pair<size_t, size_t>> implied_introns(const std::vector<std::pair<size_t, size_t>>& exons) {
    std::vector<std::pair<size_t, size_t>> introns;
    for (size_t i = 1; i < exons.size(); ++i) {
        if (exons[i - 1].second < exons[i].first) {
            introns.emplace_back(exons[i - 1].second, exons[i].first);
        }
    }
    return introns;
}

ManifestData read_manifest(const std::filesystem::path& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open collapse manifest");
    }

    std::string header;
    std::getline(in, header);
    if (header != "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id") {
        throw std::runtime_error("unexpected collapse manifest header");
    }

    ManifestData manifest;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_tab(line);
        if (fields.size() != 4) {
            throw std::runtime_error("collapse manifest row must have 4 columns");
        }
        ManifestRow row{fields[0], fields[1], fields[2], fields[3]};
        const SourceKey source_key{row.source_path, row.source_transcript};
        const auto existing_source = manifest.source_rows.find(source_key);
        if (existing_source != manifest.source_rows.end()) {
            if (existing_source->second.canonical_transcript != row.canonical_transcript ||
                existing_source->second.canonical_gene != row.canonical_gene) {
                throw std::runtime_error("manifest_contradictory_rows");
            }
            continue;
        }

        const auto existing_gene = manifest.target_gene.find(row.canonical_transcript);
        if (existing_gene != manifest.target_gene.end() && existing_gene->second != row.canonical_gene) {
            throw std::runtime_error("manifest_gene_conflicts");
        }
        manifest.source_rows.emplace(source_key, row);
        manifest.target_gene[row.canonical_transcript] = row.canonical_gene;
    }

    if (manifest.source_rows.empty()) {
        throw std::runtime_error("collapse manifest has no data rows");
    }
    if (manifest.target_gene.size() > kRadTargetIdMask) {
        throw std::runtime_error("RAD target dictionary exceeds 31-bit target IDs");
    }
    for (const auto& [target, gene] : manifest.target_gene) {
        manifest.target_ids[target] = static_cast<uint32_t>(manifest.target_names.size());
        manifest.target_names.push_back(target);
    }
    return manifest;
}

std::map<SourceKey, TranscriptModel> read_transcript_models(const std::filesystem::path& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open GTF");
    }

    std::map<SourceKey, TranscriptModel> models;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto fields = split_tab(line);
        if (fields.size() != 9 || fields[2] != "exon") {
            continue;
        }

        const std::string gene_id = gtf_attribute(fields[8], "gene_id");
        const std::string transcript_id = gtf_attribute(fields[8], "transcript_id");
        if (gene_id.empty() || transcript_id.empty()) {
            throw std::runtime_error("GTF exon is missing gene_id or transcript_id");
        }
        if (fields[6] != "+" && fields[6] != "-") {
            throw std::runtime_error("GTF exon has unsupported strand");
        }
        const size_t start_1_based = std::stoul(fields[3]);
        const size_t end_inclusive = std::stoul(fields[4]);
        if (start_1_based == 0 || end_inclusive < start_1_based) {
            throw std::runtime_error("invalid GTF exon coordinates");
        }
        const SourceKey source_key{fields[0], transcript_id};
        const bool is_reverse = fields[6] == "-";
        auto [it, inserted] = models.emplace(
            source_key, TranscriptModel{fields[0], transcript_id, gene_id, {}, {}, is_reverse});
        if (!inserted && (it->second.source_gene != gene_id || it->second.is_reverse != is_reverse)) {
            throw std::runtime_error("inconsistent GTF transcript model");
        }
        it->second.exons.emplace_back(start_1_based - 1, end_inclusive);
    }

    if (models.empty()) {
        throw std::runtime_error("GTF contains no exon model");
    }
    for (auto& [key, model] : models) {
        model.exons = merge_intervals(std::move(model.exons));
        model.introns = implied_introns(model.exons);
    }
    return models;
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
    if (length <= 4) {
        return kRadU8;
    }
    if (length <= 8) {
        return kRadU16;
    }
    if (length <= 16) {
        return kRadU32;
    }
    if (length <= 32) {
        return kRadU64;
    }
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

struct TraversalEdge {
    uint32_t next = 0;
    bool is_connection = false;
    int64_t score = 0;
};

void check_subpath_index(uint32_t index, int subpath_count) {
    if (index >= static_cast<uint32_t>(subpath_count)) {
        throw std::runtime_error("GAMP subpath edge points outside the subpath list");
    }
}

std::vector<Traversal> enumerate_traversals(const vg::MultipathAlignment& alignment,
                                            size_t& group_traversal_count,
                                            size_t traversal_cap) {
    const int subpath_count = alignment.subpath_size();
    if (subpath_count == 0) {
        throw std::runtime_error("GAMP record has no subpaths");
    }

    std::vector<std::vector<TraversalEdge>> outgoing(static_cast<size_t>(subpath_count));
    std::vector<size_t> incoming(static_cast<size_t>(subpath_count), 0);
    for (int i = 0; i < subpath_count; ++i) {
        const vg::Subpath& subpath = alignment.subpath(i);
        for (const uint32_t next : subpath.next()) {
            check_subpath_index(next, subpath_count);
            outgoing[static_cast<size_t>(i)].push_back({next, false, 0});
            ++incoming[next];
        }
        for (const vg::Connection& connection : subpath.connection()) {
            check_subpath_index(connection.next(), subpath_count);
            outgoing[static_cast<size_t>(i)].push_back(
                {connection.next(), true, static_cast<int64_t>(connection.score())});
            ++incoming[connection.next()];
        }
    }

    std::vector<uint32_t> starts;
    if (alignment.start_size() > 0) {
        for (const uint32_t start : alignment.start()) {
            check_subpath_index(start, subpath_count);
            starts.push_back(start);
        }
    } else {
        for (int i = 0; i < subpath_count; ++i) {
            if (incoming[static_cast<size_t>(i)] == 0) {
                starts.push_back(static_cast<uint32_t>(i));
            }
        }
    }
    if (starts.empty()) {
        throw std::runtime_error("GAMP traversal graph has no start subpath");
    }

    std::vector<Traversal> traversals;
    std::vector<uint32_t> path;
    std::vector<bool> incoming_connection;
    std::vector<bool> active(static_cast<size_t>(subpath_count), false);

    std::function<void(uint32_t, bool, int64_t, int64_t)> dfs =
        [&](uint32_t subpath_index, bool edge_is_connection, int64_t edge_score, int64_t prefix_score) {
            if (active[subpath_index]) {
                throw std::runtime_error("GAMP traversal graph is cyclic");
            }
            active[subpath_index] = true;
            path.push_back(subpath_index);
            incoming_connection.push_back(edge_is_connection);
            const int64_t score = prefix_score + edge_score + alignment.subpath(subpath_index).score();

            const auto& edges = outgoing[subpath_index];
            if (edges.empty()) {
                if (group_traversal_count >= traversal_cap) {
                    throw std::runtime_error("traversal_cap_exceeded_groups=1");
                }
                ++group_traversal_count;
                traversals.push_back({path, incoming_connection, score});
            } else {
                for (const TraversalEdge& edge : edges) {
                    dfs(edge.next, edge.is_connection, edge.score, score);
                }
            }

            incoming_connection.pop_back();
            path.pop_back();
            active[subpath_index] = false;
        };

    for (const uint32_t start : starts) {
        dfs(start, false, 0, 0);
    }

    return traversals;
}

bool subpath_has_reference_consuming_edit(const vg::Subpath& subpath) {
    for (const vg::Mapping& mapping : subpath.path().mapping()) {
        for (const vg::Edit& edit : mapping.edit()) {
            if (edit.from_length() < 0) {
                throw std::runtime_error("GAMP edit has negative reference length");
            }
            if (edit.from_length() > 0) {
                return true;
            }
        }
    }
    return false;
}

ProjectedTraversal project_traversal(const vg::MultipathAlignment& alignment, const Traversal& traversal, xg::XG& graph) {
    handlegraph::PathPositionHandleGraph* positioned_graph = &graph;
    ProjectedTraversal projected;
    bool pending_connection = false;

    for (size_t traversal_i = 0; traversal_i < traversal.subpaths.size(); ++traversal_i) {
        const vg::Subpath& subpath = alignment.subpath(traversal.subpaths[traversal_i]);
        pending_connection = pending_connection || traversal.incoming_connection[traversal_i];
        if (!subpath_has_reference_consuming_edit(subpath)) {
            continue;
        }
        bool first_reference_edit = true;
        for (const vg::Mapping& mapping : subpath.path().mapping()) {
            if (mapping.position().offset() < 0) {
                throw std::runtime_error("GAMP mapping has negative node offset");
            }

            const auto& position = mapping.position();
            const handlegraph::handle_t handle = graph.get_handle(position.node_id(), position.is_reverse());
            const size_t node_length = graph.get_length(handle);
            size_t edit_offset = static_cast<size_t>(position.offset());
            if (edit_offset > node_length) {
                throw std::runtime_error("GAMP mapping offset extends beyond node length");
            }

            for (const vg::Edit& edit : mapping.edit()) {
                if (edit.from_length() < 0) {
                    throw std::runtime_error("GAMP edit has negative reference length");
                }
                const size_t from_length = static_cast<size_t>(edit.from_length());
                if (from_length == 0) {
                    continue;
                }
                if (from_length > node_length || edit_offset + from_length > node_length) {
                    throw std::runtime_error("GAMP mapping edit extends beyond node length");
                }
                const size_t edit_index = projected.reference_edit_count++;
                const size_t node_start =
                    position.is_reverse() ? node_length - edit_offset - from_length : edit_offset;
                const bool edge_into_block_is_connection = first_reference_edit && pending_connection;
                positioned_graph->for_each_step_position_on_handle(
                    handle,
                    [&](const handlegraph::step_handle_t& step,
                        const bool& step_is_reverse,
                        const size_t& step_position) {
                        const std::string path_name = graph.get_path_name(graph.get_path_handle_of_step(step));
                        const bool path_step_is_reverse = graph.get_is_reverse(graph.get_handle_of_step(step));
                        const size_t start = path_step_is_reverse
                                                 ? step_position + node_length - node_start - from_length
                                                 : step_position + node_start;
                        const size_t end = start + from_length;
                        // step_is_reverse is relative to the queried handle. Its
                        // negation is the read direction relative to the source path;
                        // the path-step orientation controls coordinate mirroring.
                        projected.blocks.push_back(
                            {path_name, start, end, !step_is_reverse, edge_into_block_is_connection, edit_index});
                        return true;
                    });
                first_reference_edit = false;
                pending_connection = false;
                edit_offset += from_length;
            }
        }
    }

    return projected;
}

bool overlaps(size_t a_start, size_t a_end, size_t b_start, size_t b_end) {
    return a_start < b_end && b_start < a_end;
}

bool overlaps_any_interval(const ProjectedBlock& block, const std::vector<std::pair<size_t, size_t>>& intervals) {
    for (const auto& [interval_start, interval_end] : intervals) {
        if (overlaps(block.start, block.end, interval_start, interval_end)) {
            return true;
        }
    }
    return false;
}

bool overlaps_transcript_model(const ProjectedBlock& block, const TranscriptModel& model) {
    return overlaps_any_interval(block, model.exons) || overlaps_any_interval(block, model.introns);
}

bool has_junction(const TranscriptModel& model, size_t start, size_t end) {
    for (const auto& [intron_start, intron_end] : model.introns) {
        if (intron_start == start && intron_end == end) {
            return true;
        }
    }
    return false;
}

bool compatible_with_model(const ProjectedTraversal& traversal,
                           const TranscriptModel& model,
                           size_t min_splice_jump,
                           std::set<bool>& target_orientations) {
    std::vector<const ProjectedBlock*> model_blocks;
    std::set<size_t> projected_edit_indexes;
    for (const ProjectedBlock& block : traversal.blocks) {
        if (block.source_path == model.source_path) {
            model_blocks.push_back(&block);
            projected_edit_indexes.insert(block.edit_index);
        }
    }
    if (model_blocks.empty() || projected_edit_indexes.size() != traversal.reference_edit_count) {
        return false;
    }

    bool has_anchor = false;
    for (const ProjectedBlock* block : model_blocks) {
        target_orientations.insert(block->is_forward_on_path != model.is_reverse);
        if (overlaps_transcript_model(*block, model)) {
            has_anchor = true;
        }
    }
    if (!has_anchor) {
        return false;
    }

    for (size_t i = 1; i < model_blocks.size(); ++i) {
        const ProjectedBlock& previous = *model_blocks[i - 1];
        const ProjectedBlock& current = *model_blocks[i];

        size_t gap_start = 0;
        size_t gap_end = 0;
        bool has_gap = false;
        if (previous.end <= current.start) {
            gap_start = previous.end;
            gap_end = current.start;
            has_gap = true;
        } else if (current.end <= previous.start) {
            gap_start = current.end;
            gap_end = previous.start;
            has_gap = true;
        }

        if (!has_gap) {
            if (current.previous_edge_is_connection) {
                return false;
            }
            continue;
        }

        const size_t gap_length = gap_end - gap_start;
        if (current.previous_edge_is_connection || gap_length >= min_splice_jump) {
            if (!has_junction(model, gap_start, gap_end)) {
                return false;
            }
        }
    }

    return true;
}

void write_text_file(const std::filesystem::path& filename, const std::string& contents) {
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("cannot write " + filename.string());
    }
    out << contents;
}

int run_convert(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    const ManifestData manifest = read_manifest(options.manifest);
    const std::map<SourceKey, TranscriptModel> transcript_models = read_transcript_models(options.gtf);

    xg::XG graph;
    std::ifstream xg_in(options.xg, std::ios::binary);
    if (!xg_in) {
        throw std::runtime_error("cannot open XG");
    }
    graph.deserialize(xg_in);
    for (const auto& [source_key, row] : manifest.source_rows) {
        if (transcript_models.count(source_key) == 0) {
            throw std::runtime_error("annotation_manifest_mismatches");
        }
        if (!graph.has_path(row.source_path)) {
            throw std::runtime_error("annotation_index_consistency_failures");
        }
    }

    std::ifstream gamp_in(options.gamp, std::ios::binary);
    if (!gamp_in) {
        throw std::runtime_error("cannot open GAMP");
    }

    size_t input_records = 0;
    size_t input_read_groups = 0;
    size_t mixed_orientation_dropped_groups = 0;
    size_t grouping_recurrence_failures = 0;
    size_t no_compatible_transcript_groups = 0;
    size_t raw_molecule_missing_groups = 0;
    size_t raw_molecule_malformed_groups = 0;
    size_t raw_molecule_unsupported_groups = 0;
    size_t raw_molecule_skipped_groups = 0;
    size_t score_removed_targets = 0;
    std::vector<EmittedRecord> emitted_records;
    std::set<std::string> completed_names;
    bool have_group = false;
    ReadGroupState current_group;

    auto start_group = [&](const vg::MultipathAlignment& alignment) {
        if (completed_names.count(alignment.name()) != 0) {
            ++grouping_recurrence_failures;
            throw std::runtime_error("grouping_recurrence_failures: completed GAMP read name recurred");
        }
        current_group = {};
        current_group.name = alignment.name();
        ++input_read_groups;
        const MoleculeParseResult molecule = parse_molecule_id(alignment.name(), options.raw_cb_length, options.raw_umi_length);
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
        if (current_group.target_evidence.empty()) {
            ++no_compatible_transcript_groups;
            completed_names.insert(current_group.name);
            have_group = false;
            return;
        }

        int64_t best_score = std::numeric_limits<int64_t>::min();
        for (const auto& [target_id, evidence] : current_group.target_evidence) {
            best_score = std::max(best_score, evidence.best_score);
        }

        std::map<uint32_t, TargetEvidence> retained_targets;
        for (const auto& [target_id, evidence] : current_group.target_evidence) {
            const uint64_t score_delta = static_cast<uint64_t>(best_score - evidence.best_score);
            if (score_delta <= options.score_window) {
                retained_targets.emplace(target_id, evidence);
            } else {
                ++score_removed_targets;
            }
        }
        if (retained_targets.empty()) {
            throw std::runtime_error("internal score filtering removed all compatible targets");
        }

        bool has_mixed_target = false;
        for (const auto& [target_id, evidence] : retained_targets) {
            if (evidence.orientations.size() > 1) {
                has_mixed_target = true;
                break;
            }
        }
        if (has_mixed_target) {
            ++mixed_orientation_dropped_groups;
            std::cerr << "panCollapse: warning: dropped read group with mixed target-relative orientations: "
                      << current_group.name << '\n';
            completed_names.insert(current_group.name);
            have_group = false;
            return;
        }
        std::vector<TargetHit> hits;
        for (const auto& [target_id, evidence] : retained_targets) {
            hits.push_back({target_id, *evidence.orientations.begin()});
        }
        emitted_records.push_back({current_group.molecule, hits});
        completed_names.insert(current_group.name);
        have_group = false;
    };

    auto update_group = [&](vg::MultipathAlignment& alignment) {
        for (const Traversal& traversal :
             enumerate_traversals(alignment, current_group.traversal_count, options.max_traversals_per_read)) {
            const ProjectedTraversal projected_traversal = project_traversal(alignment, traversal, graph);
            for (const auto& [source_key, model] : transcript_models) {
                std::set<bool> target_orientations;
                if (!compatible_with_model(projected_traversal, model, options.min_splice_jump, target_orientations)) {
                    continue;
                }
                const auto manifest_row = manifest.source_rows.find(source_key);
                if (manifest_row == manifest.source_rows.end()) {
                    throw std::runtime_error("manifest_missing_source_identities");
                }
                const auto target_id = manifest.target_ids.find(manifest_row->second.canonical_transcript);
                if (target_id == manifest.target_ids.end()) {
                    throw std::runtime_error("internal target dictionary mismatch");
                }
                TargetEvidence& evidence = current_group.target_evidence[target_id->second];
                evidence.best_score = std::max(evidence.best_score, traversal.score);
                evidence.orientations.insert(target_orientations.begin(), target_orientations.end());
            }
        }
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
            update_group(alignment);
        }
    });
    flush_group();

    if (input_records == 0) {
        throw std::runtime_error("panCollapse convert expects at least one GAMP record");
    }

    std::filesystem::create_directories(options.out_dir);
    write_rad_file(options.out_dir / "map.rad",
                   manifest.target_names,
                   emitted_records,
                   options.raw_cb_length,
                   options.raw_umi_length);
    std::string tx2gene;
    for (const std::string& target_name : manifest.target_names) {
        tx2gene += target_name + '\t' + manifest.target_gene.at(target_name) + '\n';
    }
    write_text_file(options.out_dir / "tx2gene.tsv", tx2gene);
    write_text_file(options.out_dir / "summary.tsv",
                    "input_records\t" + std::to_string(input_records) + "\ninput_read_groups\t" +
                        std::to_string(input_read_groups) + "\nemitted_groups\t" + std::to_string(emitted_records.size()) +
                        "\nmixed_orientation_dropped_groups\t" + std::to_string(mixed_orientation_dropped_groups) +
                        "\nno_compatible_transcript_groups\t" + std::to_string(no_compatible_transcript_groups) +
                        "\nraw_molecule_missing_groups\t" + std::to_string(raw_molecule_missing_groups) +
                        "\nraw_molecule_malformed_groups\t" + std::to_string(raw_molecule_malformed_groups) +
                        "\nraw_molecule_unsupported_groups\t" + std::to_string(raw_molecule_unsupported_groups) +
                        "\nraw_molecule_skipped_groups\t" + std::to_string(raw_molecule_skipped_groups) +
                        "\nscore_removed_targets\t" + std::to_string(score_removed_targets) +
                        "\ntraversal_cap_exceeded_groups\t0" +
                        "\ngrouping_recurrence_failures\t" + std::to_string(grouping_recurrence_failures) + "\n");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run_convert(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "panCollapse: " << e.what() << '\n';
        return 1;
    }
}
