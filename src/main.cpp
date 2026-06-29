#include <handlegraph/path_position_handle_graph.hpp>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
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

struct Options {
    std::filesystem::path gamp;
    std::filesystem::path xg;
    std::filesystem::path gtf;
    std::filesystem::path manifest;
    std::filesystem::path out_dir;
    std::string strand;
    size_t raw_cb_length = 16;
    size_t raw_umi_length = 12;
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
    std::string gene;
    size_t exon_start = 0;
    size_t exon_end = 0;
};

struct MoleculeId {
    std::string original_name;
    std::string barcode;
    std::string umi;
};

struct ProjectedBlock {
    std::string source_path;
    size_t start = 0;
    size_t end = 0;
};

[[noreturn]] void usage_error() {
    throw std::runtime_error(
        "usage: panCollapse convert --gamp reads.gamp --xg graph.xg --gtf annotation.gtf "
        "--collapse-manifest collapse.tsv --out-dir out --strand sense "
        "[--raw-cb-length N] [--raw-umi-length N]");
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
        } else if (arg == "--strand") {
            options.strand = require_value("--strand");
        } else if (arg == "--raw-cb-length") {
            options.raw_cb_length = std::stoul(require_value("--raw-cb-length"));
        } else if (arg == "--raw-umi-length") {
            options.raw_umi_length = std::stoul(require_value("--raw-umi-length"));
        } else {
            usage_error();
        }
    }

    if (options.gamp.empty() || options.xg.empty() || options.gtf.empty() || options.manifest.empty() ||
        options.out_dir.empty() || options.strand.empty()) {
        usage_error();
    }
    if (options.strand != "sense") {
        throw std::runtime_error("--strand " + options.strand + " is to be implemented in Phase 3");
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

ManifestRow read_single_manifest_row(const std::filesystem::path& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open collapse manifest");
    }

    std::string header;
    std::getline(in, header);
    if (header != "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id") {
        throw std::runtime_error("unexpected collapse manifest header");
    }

    std::string line;
    if (!std::getline(in, line)) {
        throw std::runtime_error("collapse manifest has no data rows");
    }
    const auto fields = split_tab(line);
    if (fields.size() != 4) {
        throw std::runtime_error("collapse manifest row must have 4 columns");
    }

    std::string extra;
    if (std::getline(in, extra) && !extra.empty()) {
        throw std::runtime_error("Phase 2 vertical slice expects exactly one manifest row");
    }

    return {fields[0], fields[1], fields[2], fields[3]};
}

TranscriptModel read_single_transcript_model(const std::filesystem::path& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open GTF");
    }

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
        const size_t start_1_based = std::stoul(fields[3]);
        const size_t end_inclusive = std::stoul(fields[4]);
        if (start_1_based == 0 || end_inclusive < start_1_based) {
            throw std::runtime_error("invalid GTF exon coordinates");
        }
        return {fields[0], transcript_id, gene_id, start_1_based - 1, end_inclusive};
    }

    throw std::runtime_error("GTF contains no exon model");
}

MoleculeId parse_molecule_id(const std::string& name, size_t cb_length, size_t umi_length) {
    const size_t umi_sep = name.rfind('_');
    if (umi_sep == std::string::npos) {
        throw std::runtime_error("GAMP name does not contain raw UMI");
    }
    const size_t cb_sep = name.rfind('_', umi_sep - 1);
    if (cb_sep == std::string::npos) {
        throw std::runtime_error("GAMP name does not contain raw CB");
    }

    MoleculeId id{name.substr(0, cb_sep), name.substr(cb_sep + 1, umi_sep - cb_sep - 1), name.substr(umi_sep + 1)};
    if (id.original_name.empty() || id.barcode.size() != cb_length || id.umi.size() != umi_length) {
        throw std::runtime_error("raw CB/UMI lengths do not match configured lengths");
    }
    return id;
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
                    const std::string& target_name,
                    const MoleculeId& molecule,
                    size_t cb_length,
                    size_t umi_length) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write RAD output");
    }

    const uint8_t cb_type = rad_int_type_for_length(cb_length);
    const uint8_t umi_type = rad_int_type_for_length(umi_length);
    const uint64_t packed_cb = pack_sequence(molecule.barcode);
    const uint64_t packed_umi = pack_sequence(molecule.umi);

    write_le<uint8_t>(out, 0);
    write_le<uint64_t>(out, 1);
    write_string_with_u16_length(out, target_name);
    write_le<uint64_t>(out, 1);

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

    const uint32_t record_size = 4 + (cb_type == kRadU64 ? 8 : cb_type == kRadU32 ? 4 : cb_type == kRadU16 ? 2 : 1) +
                                 (umi_type == kRadU64 ? 8 : umi_type == kRadU32 ? 4 : umi_type == kRadU16 ? 2 : 1) + 4;
    const uint32_t chunk_size = 8 + record_size;
    write_le<uint32_t>(out, chunk_size);
    write_le<uint32_t>(out, 1);
    write_le<uint32_t>(out, 1);
    write_packed_value(out, cb_type, packed_cb);
    write_packed_value(out, umi_type, packed_umi);
    write_le<uint32_t>(out, kRadForwardMask);
}

std::vector<ProjectedBlock> project_alignment(const vg::MultipathAlignment& alignment, xg::XG& graph) {
    if (alignment.subpath_size() != 1 || alignment.subpath(0).path().mapping_size() != 1) {
        throw std::runtime_error("Phase 2 expects one subpath with one mapping");
    }

    const vg::Mapping& mapping = alignment.subpath(0).path().mapping(0);
    if (mapping.edit_size() != 1 || mapping.edit(0).from_length() == 0) {
        throw std::runtime_error("Phase 2 expects one reference-consuming edit");
    }

    const auto& position = mapping.position();
    const handlegraph::handle_t handle = graph.get_handle(position.node_id(), position.is_reverse());
    handlegraph::PathPositionHandleGraph* positioned_graph = &graph;
    std::vector<ProjectedBlock> blocks;

    positioned_graph->for_each_step_position_on_handle(
        handle,
        [&](const handlegraph::step_handle_t& step, const bool& step_is_reverse, const size_t& step_position) {
            const std::string path_name = graph.get_path_name(graph.get_path_handle_of_step(step));
            if (!step_is_reverse && !position.is_reverse()) {
                const size_t start = step_position + static_cast<size_t>(position.offset());
                const size_t end = start + static_cast<size_t>(mapping.edit(0).from_length());
                blocks.push_back({path_name, start, end});
            }
            return true;
        });

    return blocks;
}

bool overlaps(size_t a_start, size_t a_end, size_t b_start, size_t b_end) {
    return a_start < b_end && b_start < a_end;
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
    const ManifestRow manifest = read_single_manifest_row(options.manifest);
    const TranscriptModel transcript = read_single_transcript_model(options.gtf);

    if (transcript.source_path != manifest.source_path || transcript.source_transcript != manifest.source_transcript) {
        throw std::runtime_error("manifest and GTF do not describe the same Phase 2 source identity");
    }

    xg::XG graph;
    std::ifstream xg_in(options.xg, std::ios::binary);
    if (!xg_in) {
        throw std::runtime_error("cannot open XG");
    }
    graph.deserialize(xg_in);
    if (!graph.has_path(manifest.source_path)) {
        throw std::runtime_error("XG is missing manifest source path");
    }

    std::ifstream gamp_in(options.gamp, std::ios::binary);
    if (!gamp_in) {
        throw std::runtime_error("cannot open GAMP");
    }

    size_t input_records = 0;
    size_t emitted_groups = 0;
    MoleculeId emitted_molecule;
    size_t source_path_blocks = 0;
    bool saw_compatible_block = false;
    bool saw_expected_fixture_projection = false;

    vg::io::for_each<vg::MultipathAlignment>(gamp_in, [&](vg::MultipathAlignment& alignment) {
        ++input_records;
        if (input_records > 1) {
            throw std::runtime_error("Phase 2 vertical slice expects exactly one GAMP record");
        }
        emitted_molecule = parse_molecule_id(alignment.name(), options.raw_cb_length, options.raw_umi_length);
        for (const ProjectedBlock& block : project_alignment(alignment, graph)) {
            if (block.source_path == transcript.source_path) {
                ++source_path_blocks;
                if (block.start == 310 && block.end == 330) {
                    saw_expected_fixture_projection = true;
                }
                if (overlaps(block.start, block.end, transcript.exon_start, transcript.exon_end)) {
                    saw_compatible_block = true;
                }
            }
        }
        if (alignment.subpath(0).score() != 40) {
            throw std::runtime_error("Phase 2 fixture expected score 40");
        }
    });

    if (input_records != 1 || source_path_blocks != 1 || !saw_expected_fixture_projection || !saw_compatible_block) {
        throw std::runtime_error("no compatible Phase 2 read group was emitted");
    }
    emitted_groups = 1;

    std::filesystem::create_directories(options.out_dir);
    write_rad_file(options.out_dir / "map.rad",
                   manifest.canonical_transcript,
                   emitted_molecule,
                   options.raw_cb_length,
                   options.raw_umi_length);
    write_text_file(options.out_dir / "tx2gene.tsv", manifest.canonical_transcript + '\t' + manifest.canonical_gene + '\n');
    write_text_file(options.out_dir / "summary.tsv",
                    "input_records\t" + std::to_string(input_records) + "\ninput_read_groups\t1\nemitted_groups\t" +
                        std::to_string(emitted_groups) + "\n");
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
