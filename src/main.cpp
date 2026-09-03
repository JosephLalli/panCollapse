#include <handlegraph/path_position_handle_graph.hpp>
#include <handlegraph/util.hpp>
#include <htslib/sam.h>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include "pathtally.hpp"
#include "pathtally_ledger.hpp"
#include "pathtally_qualadj.hpp"
#include "path_identity_ledger.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <thread>
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

// Exact GeneFull_Ex50pAS first selects complete transcript-compatible traversals whose mapper
// score is within one mismatch-equivalent of the global compatible optimum. Under vg's default
// scoring, replacing one +1 match with one -4 mismatch changes the score by five points.
constexpr int64_t kExactEx50ScoreWindow = 5;

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
    std::filesystem::path path_identity_ledger;
    std::string legacy_adapter;
    std::filesystem::path out_dir;
    size_t raw_cb_length = 16;
    size_t raw_umi_length = 12;
    MoleculeIdentityFailurePolicy molecule_identity_failures = MoleculeIdentityFailurePolicy::Skip;
    ScoreMode score_mode = ScoreMode::Flat;
    std::filesystem::path bam_out;  // empty => no BAM output
    BamMultiGenePolicy bam_multigene = BamMultiGenePolicy::Omit;
    StrandFilter strand = StrandFilter::Both;
    pathtally::CountMode count_mode = pathtally::CountMode::Score;
    std::filesystem::path body_t2g;  // hst-v1 gene-body layer, required for adapter count modes
    std::filesystem::path debug_evidence_out;  // empty => no audit-only TSV sidecar
    size_t threads = 1;  // complete read-name group workers; parsing/writing stay ordered
    // Optional projected exact-count BAM. The value is the library orientation used to select
    // the single global Ex50pAS winning rank before serialization. This is deliberately separate
    // from --strand, which continues to control the RAD target filter.
    std::optional<StrandFilter> compact_exact_count_strand;
    std::string path_identity_ledger_sha256;
    // Reference-build compatibility is a pre-score-window Parent filter. The allowlist is derived
    // from the same annotation generation as the path ledger and is bound into the BAM header.
    std::filesystem::path strict_allowlisted_parents;
    std::string strict_allowlisted_parents_sha256;
    // D068 is the v0.8 default. The explicit opt-out restores the pre-D068 exact-evidence
    // eligibility surface: every complete compatible traversal reaches downstream Ex50 ranking.
    bool exact_ex50_score_window_enabled = true;
};

struct MoleculeId {
    std::string original_name;
    std::string barcode;
    std::string umi;
    std::string barcode_quality;
    std::string umi_quality;
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

// Internal path -> canonical transcript -> gene projection. Production populates it from the
// strict identity ledger; hst-v1 uses the historical t2g parser below.
struct T2gData {
    std::map<std::string, std::string> path_transcript;
    // Production-ledger-only exact path provenance. Empty under hst-v1.
    std::map<std::string, std::string> path_unique_parent;
    std::map<std::string, std::string> transcript_gene;
    // Only canonical targets introduced by a legacy two-column row may match additional
    // graph paths through terminal _H<n>/_R<n> stripping in hst-v1 count mode. Three-column rows
    // are exact raw-path aliases and must never enable that fallback implicitly.
    std::unordered_set<std::string> legacy_fallback_transcripts;
    std::map<std::string, uint32_t> target_ids;
    std::vector<std::string> target_names;
    // Sorted unique gene ids and gene -> index, used only for the optional BAM output:
    // one @SQ contig per gene, in this order.
    std::vector<std::string> gene_names;
    std::map<std::string, int32_t> gene_ids;
};

constexpr const char* kUsageText =
    "usage: panCollapse convert --gamp reads.gamp|- --xg graph.xg "
    "(--path-identity-ledger path_identity_ledger.tsv | "
    "--legacy-adapter hst-v1 --t2g t2g.tsv) "
    "--out-dir out [--raw-cb-length N] [--raw-umi-length N] "
    "[--score flat|qualadj] [--molecule-identity-failures skip|fail] "
    "[--strand both|forward|reverse] "
    "[--count-mode score|gene|genefull|genefull_exonoverintron|genefull_ex50pas] "
    "[--body-t2g body.t2g] "
    "[--threads N] "
    "[--bam-out reads.bam] [--bam-multigene omit|first|all] "
    "[--debug-evidence-out audit.tsv] [--no-ex50-score-window] "
    "[--compact-exact-count-bam forward|reverse] "
    "[--path-identity-ledger-sha256 HEX] "
    "[--strict-allowlisted-parents parents.txt] "
    "[--strict-allowlisted-parents-sha256 HEX]";

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
        } else if (arg == "--path-identity-ledger") {
            options.path_identity_ledger = require_value("--path-identity-ledger");
        } else if (arg == "--legacy-adapter") {
            options.legacy_adapter = require_value("--legacy-adapter");
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
        } else if (arg == "--debug-evidence-out") {
            options.debug_evidence_out = require_value("--debug-evidence-out");
        } else if (arg == "--threads") {
            options.threads = parse_size_option("--threads", require_value("--threads"));
        } else if (arg == "--no-ex50-score-window") {
            options.exact_ex50_score_window_enabled = false;
        } else if (arg == "--compact-exact-count-bam") {
            const std::string value = require_value("--compact-exact-count-bam");
            if (value == "forward") {
                options.compact_exact_count_strand = StrandFilter::Forward;
            } else if (value == "reverse") {
                options.compact_exact_count_strand = StrandFilter::Reverse;
            } else {
                throw std::runtime_error(
                    "--compact-exact-count-bam must be forward or reverse");
            }
        } else if (arg == "--path-identity-ledger-sha256") {
            options.path_identity_ledger_sha256 =
                require_value("--path-identity-ledger-sha256");
        } else if (arg == "--strict-allowlisted-parents") {
            options.strict_allowlisted_parents =
                require_value("--strict-allowlisted-parents");
        } else if (arg == "--strict-allowlisted-parents-sha256") {
            options.strict_allowlisted_parents_sha256 =
                require_value("--strict-allowlisted-parents-sha256");
        } else {
            usage_error();
        }
    }

    if (options.gamp.empty() || options.xg.empty() || options.out_dir.empty()) {
        usage_error();
    }
    if (options.raw_cb_length == 0 || options.raw_cb_length > 32 || options.raw_umi_length == 0 ||
        options.raw_umi_length > 32) {
        throw std::runtime_error("raw barcode and UMI lengths must be in 1..32");
    }
    if (options.threads == 0) {
        throw std::runtime_error("--threads must be at least 1");
    }
    const bool production_ledger = !options.path_identity_ledger.empty();
    const bool legacy_adapter = !options.legacy_adapter.empty();
    if (production_ledger == legacy_adapter) {
        throw std::runtime_error(
            "select exactly one identity input: --path-identity-ledger or --legacy-adapter hst-v1");
    }
    if (options.count_mode == pathtally::CountMode::GeneFullEx50pAS) {
        if (!production_ledger) {
            throw std::runtime_error(
                "--count-mode genefull_ex50pas requires --path-identity-ledger; "
                "legacy t2g/body-t2g evidence cannot represent exact base-overlap tiers");
        }
        if (options.bam_out.empty()) {
            throw std::runtime_error(
                "--count-mode genefull_ex50pas is a BAM/count_cr mode and requires --bam-out");
        }
        if (options.bam_multigene != BamMultiGenePolicy::All) {
            throw std::runtime_error(
                "--count-mode genefull_ex50pas requires --bam-multigene all so downstream "
                "selection receives every exact evidence entry");
        }
    }
    if (!options.exact_ex50_score_window_enabled &&
        (!production_ledger || options.count_mode == pathtally::CountMode::Score ||
         options.bam_out.empty())) {
        throw std::runtime_error(
            "--no-ex50-score-window applies only to a production-ledger count-mode BAM");
    }
    if (options.compact_exact_count_strand.has_value() &&
        options.count_mode != pathtally::CountMode::GeneFullEx50pAS) {
        throw std::runtime_error(
            "--compact-exact-count-bam applies only to --count-mode genefull_ex50pas");
    }
    auto valid_sha256 = [](const std::string& digest) {
        return digest.size() == 64 &&
            std::all_of(digest.begin(), digest.end(), [](const char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            });
    };
    if (options.compact_exact_count_strand.has_value()) {
        if (!valid_sha256(options.path_identity_ledger_sha256)) {
            throw std::runtime_error(
                "--compact-exact-count-bam requires --path-identity-ledger-sha256 "
                "as 64 lowercase hexadecimal characters");
        }
    } else if (!options.path_identity_ledger_sha256.empty()) {
        throw std::runtime_error(
            "--path-identity-ledger-sha256 applies only with --compact-exact-count-bam");
    }
    if (!options.strict_allowlisted_parents.empty()) {
        if (options.count_mode != pathtally::CountMode::GeneFullEx50pAS) {
            throw std::runtime_error(
                "--strict-allowlisted-parents applies only to --count-mode genefull_ex50pas");
        }
        if (!valid_sha256(options.strict_allowlisted_parents_sha256)) {
            throw std::runtime_error(
                "--strict-allowlisted-parents requires "
                "--strict-allowlisted-parents-sha256 as 64 lowercase hexadecimal characters");
        }
    } else if (!options.strict_allowlisted_parents_sha256.empty()) {
        throw std::runtime_error(
            "--strict-allowlisted-parents-sha256 requires --strict-allowlisted-parents");
    }
    if (!options.debug_evidence_out.empty()) {
        if (!production_ledger ||
            options.count_mode != pathtally::CountMode::GeneFullEx50pAS ||
            options.bam_out.empty() ||
            options.bam_multigene != BamMultiGenePolicy::All) {
            throw std::runtime_error(
                "--debug-evidence-out is an audit sidecar for the production superset conversion "
                "and requires --path-identity-ledger, --count-mode genefull_ex50pas, --bam-out, "
                "and --bam-multigene all");
        }
        if (options.debug_evidence_out == options.bam_out) {
            throw std::runtime_error("--debug-evidence-out must differ from --bam-out");
        }
    }
    if (production_ledger) {
        if (!options.t2g.empty() || !options.body_t2g.empty()) {
            throw std::runtime_error(
                "--path-identity-ledger cannot be combined with --t2g or --body-t2g");
        }
    } else {
        if (options.legacy_adapter != "hst-v1") {
            throw std::runtime_error("the only supported --legacy-adapter is hst-v1");
        }
        if (options.t2g.empty()) {
            throw std::runtime_error("--legacy-adapter hst-v1 requires --t2g");
        }
        // The adapter preserves the historical two-file count-mode contract exactly.
        if (options.count_mode == pathtally::CountMode::Score) {
            if (!options.body_t2g.empty()) {
                throw std::runtime_error("--body-t2g is only used by hst-v1 count modes");
            }
        } else if (options.body_t2g.empty()) {
            throw std::runtime_error("--count-mode gene/genefull/... requires --body-t2g");
        }
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

std::unordered_set<std::string> read_strict_allowlisted_parents(
    const std::filesystem::path& filename) {
    std::ifstream input(filename);
    if (!input) {
        throw std::runtime_error("cannot open strict allowlisted Parent file");
    }
    std::unordered_set<std::string> parents;
    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.find_first_of("\t ") != std::string::npos) {
            throw std::runtime_error(
                "strict allowlisted Parent file line " + std::to_string(line_number) +
                " must contain exactly one Parent identity");
        }
        if (!parents.insert(line).second) {
            throw std::runtime_error(
                "duplicate strict allowlisted Parent " + line + " at line " +
                std::to_string(line_number));
        }
    }
    if (parents.empty()) {
        throw std::runtime_error("strict allowlisted Parent file is empty");
    }
    return parents;
}

void finalize_t2g(T2gData& data) {
    if (data.transcript_gene.empty()) {
        throw std::runtime_error("identity input has no exon data rows");
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
        if ((fields.size() != 2 && fields.size() != 3) || fields[0].empty() || fields[1].empty() ||
            (fields.size() == 3 && fields[2].empty())) {
            throw std::runtime_error(
                "t2g row must have path, gene, and optional canonical-transcript columns");
        }
        const std::string& hst_name = fields[0];
        const std::string& gene = fields[1];
        const std::string transcript =
            fields.size() == 3 ? fields[2] : pathtally::transcript_id_of(hst_name);
        if (fields.size() == 2) {
            data.legacy_fallback_transcripts.insert(transcript);
        }
        auto [path_it, path_inserted] = data.path_transcript.emplace(hst_name, transcript);
        if (!path_inserted && path_it->second != transcript) {
            throw std::runtime_error("t2g maps path " + hst_name + " to multiple transcripts");
        }
        auto [it, inserted] = data.transcript_gene.emplace(transcript, gene);
        if (!inserted && it->second != gene) {
            throw std::runtime_error("t2g maps transcript " + transcript + " to multiple genes");
        }
    }
    finalize_t2g(data);
    return data;
}

// Resolve a graph path to its canonical transcript. Exact raw-path aliases are always
// authoritative. The hst-v1 count adapter additionally retains its historical bare-t2g fallback:
// after stripping _H<n>/_R<n>, accept either a bare raw t2g row or an already-canonical
// two-column transcript id. Score mode passes allow_suffix_fallback=false and therefore
// continues to score only graph paths explicitly named in the t2g.
const std::string* resolve_graph_transcript(const T2gData& data, const std::string& path_name,
                                            bool allow_suffix_fallback) {
    const auto exact = data.path_transcript.find(path_name);
    if (exact != data.path_transcript.end()) {
        return &exact->second;
    }
    if (!allow_suffix_fallback) {
        return nullptr;
    }
    const std::string bare = pathtally::transcript_id_of(path_name);
    if (bare == path_name) {
        return nullptr;
    }
    if (data.legacy_fallback_transcripts.count(bare) == 0) {
        return nullptr;
    }
    const auto canonical = data.transcript_gene.find(bare);
    return canonical == data.transcript_gene.end() ? nullptr : &canonical->first;
}

struct BodyPathTarget {
    std::string gene;
    // Empty for the legacy two-column gene-body contract. In the three-column contract this is
    // the canonical transcript whose unspliced body the raw graph path represents.
    std::string transcript;
};

struct BodyT2gData {
    std::map<std::string, BodyPathTarget> paths;
    // Production-ledger-only exact path provenance. Empty under hst-v1.
    std::map<std::string, std::string> path_unique_parent;
    std::map<std::string, std::string> transcript_gene;
    bool transcript_specific = false;
};

// Ledger body annotation. A consistently two-column file preserves the legacy raw
// path->gene body layer. A consistently three-column file is transcript-specific:
// raw_body_path<TAB>gene<TAB>canonical_transcript. Mixing row widths would silently mix two
// different classification algorithms, so it is rejected.
BodyT2gData read_body_t2g(const std::filesystem::path& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open t2g " + filename.string());
    }
    BodyT2gData data;
    std::optional<bool> transcript_specific;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto fields = split_tab(line);
        if ((fields.size() != 2 && fields.size() != 3) || fields[0].empty() || fields[1].empty() ||
            (fields.size() == 3 && fields[2].empty())) {
            throw std::runtime_error(
                "body t2g row must have path, gene, and optional canonical-transcript columns");
        }
        const bool row_is_transcript_specific = fields.size() == 3;
        if (!transcript_specific.has_value()) {
            transcript_specific = row_is_transcript_specific;
        } else if (*transcript_specific != row_is_transcript_specific) {
            throw std::runtime_error("body t2g cannot mix two-column and three-column rows");
        }
        const std::string transcript = row_is_transcript_specific ? fields[2] : std::string{};
        auto [it, inserted] = data.paths.emplace(fields[0], BodyPathTarget{fields[1], transcript});
        if (!inserted &&
            (it->second.gene != fields[1] || it->second.transcript != transcript)) {
            throw std::runtime_error("body t2g maps path " + fields[0] + " inconsistently");
        }
        if (row_is_transcript_specific) {
            auto [tg, tg_inserted] = data.transcript_gene.emplace(transcript, fields[1]);
            if (!tg_inserted && tg->second != fields[1]) {
                throw std::runtime_error("body t2g maps transcript " + transcript +
                                         " to multiple genes");
            }
        }
    }
    if (data.paths.empty()) {
        throw std::runtime_error("t2g has no data rows: " + filename.string());
    }
    data.transcript_specific = transcript_specific.value_or(false);
    return data;
}

// Exact body-path rows are authoritative. Only the legacy two-column format retains the
// suffix-stripped bare-path fallback; explicit transcript-body rows must name the actual raw
// graph paths so a fragment/copy cannot be assigned accidentally.
const BodyPathTarget* resolve_body_graph_path(const BodyT2gData& data,
                                              const std::string& path_name) {
    const auto exact = data.paths.find(path_name);
    if (exact != data.paths.end()) {
        return &exact->second;
    }
    if (data.transcript_specific) {
        return nullptr;
    }
    const std::string bare = pathtally::transcript_id_of(path_name);
    if (bare == path_name) {
        return nullptr;
    }
    const auto fallback = data.paths.find(bare);
    return fallback == data.paths.end() ? nullptr : &fallback->second;
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
    // New RNA carry-along names end in _cy<hex(CY)>_uy<hex(UY)>. Hex keeps arbitrary printable
    // FASTQ quality characters out of the QNAME delimiter/whitespace grammar. Legacy names with
    // neither quality, and the transitional CY-only form, remain accepted.
    std::string molecule_name = name;
    std::string barcode_quality;
    std::string umi_quality;
    auto decode_quality_suffix = [&](const std::string& prefix, size_t expected_length,
                                     std::string& decoded, const std::string& label) {
        const size_t quality_sep = molecule_name.rfind('_');
        if (quality_sep == std::string::npos ||
            molecule_name.compare(quality_sep + 1, prefix.size(), prefix) != 0) {
            return false;
        }
        const std::string encoded = molecule_name.substr(quality_sep + 1 + prefix.size());
        if (encoded.size() != expected_length * 2) {
            throw std::invalid_argument("hex-encoded raw " + label +
                                        " quality length does not match configured length");
        }
        auto hex_value = [](char value) -> int {
            if (value >= '0' && value <= '9') {
                return value - '0';
            }
            value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
            return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
        };
        decoded.reserve(expected_length);
        for (size_t i = 0; i < encoded.size(); i += 2) {
            const int hi = hex_value(encoded[i]);
            const int lo = hex_value(encoded[i + 1]);
            if (hi < 0 || lo < 0) {
                throw std::invalid_argument("raw " + label +
                                            " quality contains non-hexadecimal text");
            }
            const char quality = static_cast<char>((hi << 4) | lo);
            const unsigned char printable = static_cast<unsigned char>(quality);
            if (printable < 33 || printable > 126) {
                throw std::invalid_argument("decoded raw " + label +
                                            " quality is not printable FASTQ quality text");
            }
            decoded.push_back(quality);
        }
        molecule_name.resize(quality_sep);
        return true;
    };
    try {
        const bool has_uy = decode_quality_suffix("uy", umi_length, umi_quality, "UMI");
        const bool has_cy = decode_quality_suffix("cy", cb_length, barcode_quality, "barcode");
        if (has_uy && !has_cy) {
            return {MoleculeParseStatus::Malformed, {},
                    "raw UMI quality suffix requires the preceding barcode quality suffix"};
        }
    } catch (const std::invalid_argument& error) {
        return {MoleculeParseStatus::Malformed, {}, error.what()};
    }

    const size_t umi_sep = molecule_name.rfind('_');
    if (umi_sep == std::string::npos) {
        return {MoleculeParseStatus::Missing, {}, "GAMP name does not contain raw UMI"};
    }
    if (umi_sep == 0 || umi_sep + 1 == molecule_name.size()) {
        return {MoleculeParseStatus::Missing, {}, "GAMP name has an empty raw CB or UMI field"};
    }
    const size_t cb_sep = molecule_name.rfind('_', umi_sep - 1);
    if (cb_sep == std::string::npos) {
        return {MoleculeParseStatus::Missing, {}, "GAMP name does not contain raw CB"};
    }

    MoleculeId id{molecule_name.substr(0, cb_sep),
                  molecule_name.substr(cb_sep + 1, umi_sep - cb_sep - 1),
                  molecule_name.substr(umi_sep + 1), barcode_quality, umi_quality};
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
// Feature records carry panCollapse's graph-derived gene assignment as 10x tags, not real
// alignment coordinates: the counter ignores position, so they are placed nominally at position
// 1 of a synthetic per-gene contig. A valid group with no count feature instead gets one unmapped
// barcode-only record. This keeps the barcode-correction population independent of feature
// construction without manufacturing gene evidence. Records are written in GAMP order
// (@HD SO:unsorted).
class BamWriter {
public:
    // One synthetic contig per gene needs a length; the value is nominal (position is ignored
    // downstream) but must exceed any read length so a POS=1 full-length M CIGAR stays valid.
    static constexpr int32_t kNominalContigLength = 1 << 28;

    BamWriter(const std::filesystem::path& filename, const std::vector<std::string>& gene_names,
              const std::string& version, const std::string& command_line, bool typed_union,
              bool exact_ex50_score_window_enabled,
              const std::string& compact_exact_count_strand,
              const std::string& path_identity_ledger_sha256,
              const std::string& strict_allowlisted_parents_sha256,
              size_t raw_cb_length, size_t raw_umi_length)
        : final_path_(filename), temp_path_(filename.string() + ".tmp"),
          compact_exact_(!compact_exact_count_strand.empty()) {
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
        std::string compatibility_comments;
        if (!strict_allowlisted_parents_sha256.empty()) {
            compatibility_comments =
                "@CO\tpanCollapse-compatible-parent-policy:strict-allowlisted-v1\n"
                "@CO\tpanCollapse-compatible-parent-allowlist-sha256:" +
                strict_allowlisted_parents_sha256 + "\n";
        }
        if (compact_exact_) {
            const std::string comments =
                "@CO\tpanCollapse-evidence-schema:panCollapse-exact-count-v1\n"
                "@CO\tpanCollapse-exact-count-strand:" + compact_exact_count_strand + "\n"
                "@CO\tpanCollapse-exact-count-candidates:both\n"
                "@CO\tpanCollapse-exact-count-direction:target-relative-FR\n"
                "@CO\tpanCollapse-raw-cb-length:" + std::to_string(raw_cb_length) + "\n"
                "@CO\tpanCollapse-raw-umi-length:" + std::to_string(raw_umi_length) + "\n"
                "@CO\tpanCollapse-path-identity-ledger-sha256:" +
                path_identity_ledger_sha256 + "\n"
                "@CO\tpanCollapse-ex50-score-window:" +
                (exact_ex50_score_window_enabled
                     ? std::to_string(kExactEx50ScoreWindow)
                     : std::string("disabled")) +
                "\n" + compatibility_comments;
            if (sam_hdr_add_lines(hdr_, comments.c_str(), 0) < 0) {
                throw std::runtime_error("cannot write BAM compact exact-count schema markers");
            }
        } else if (typed_union) {
            const std::string comments =
                "@CO\tpanCollapse-evidence-schema:panCollapse-superset-v1\n"
                "@CO\tpanCollapse-ex50-score-window:" +
                (exact_ex50_score_window_enabled
                     ? std::to_string(kExactEx50ScoreWindow)
                     : std::string("disabled")) +
                "\n" + compatibility_comments;
            if (sam_hdr_add_lines(hdr_, comments.c_str(), 0) < 0) {
                throw std::runtime_error("cannot write BAM typed-union schema marker");
            }
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

    // One nominal feature record, or one unmapped barcode-only record when gene_tid is -1. The latter
    // lets downstream correction compute STARsolo's pre-alignment all-read barcode prior without
    // turning a featureless read into gene evidence. xt is written only when has_xt is true; optional
    // identity/state tags are written only when non-empty.
    void write_record(const std::string& qname, int32_t gene_tid, const std::string& seq,
                      const std::string& qual, const std::string& cb, const std::string& ub,
                      const std::string& cy, const std::string& uy, const std::string& gx,
                      const std::string& gn,
                      const std::string& xt, bool has_xt, const std::string& gd,
                      const std::string& gl = "",
                      const std::string& gt = "",
                      const std::string& tx = "", const std::string& xp = "",
                      const std::string& xu = "", const std::string& xr = "") {
        if (!xr.empty()) {
            const auto field_count = [](const std::string& value) {
                return static_cast<size_t>(1 + std::count(value.begin(), value.end(), ';'));
            };
            const size_t n = field_count(xr);
            if (field_count(tx) != n || field_count(gx) != n || field_count(gd) != n ||
                field_count(gl) != n || field_count(gt) != n || field_count(xp) != n ||
                field_count(xu) != n) {
                throw std::runtime_error("typed-union BAM row vectors must have equal lengths");
            }
        }
        const size_t l_seq = seq.size();
        const uint32_t cigar = (static_cast<uint32_t>(l_seq) << BAM_CIGAR_SHIFT) | BAM_CMATCH;
        const bool have_qual = !qual.empty() && qual.size() == l_seq;
        const bool barcode_only = gene_tid < 0;
        if (bam_set1(rec_, qname.size(), qname.c_str(),
                     /*flag=*/barcode_only ? BAM_FUNMAP : 0,
                     /*tid=*/barcode_only ? -1 : gene_tid,
                     /*pos=*/barcode_only ? -1 : 0,
                     /*mapq=*/barcode_only ? 0 : 255,
                     !barcode_only && l_seq > 0 ? 1 : 0,
                     !barcode_only && l_seq > 0 ? &cigar : nullptr,
                     /*mtid=*/-1, /*mpos=*/-1, /*isize=*/0, l_seq, l_seq > 0 ? seq.data() : nullptr,
                     have_qual ? qual.data() : nullptr, /*l_aux=*/0) < 0) {
            throw std::runtime_error("cannot build BAM record");
        }
        append_tag("CB", cb);
        append_tag("CR", cb);  // panCollapse carries only raw values, so raw == the CB/UB values
        append_tag("UB", ub);
        append_tag("UR", ub);
        if (!cy.empty()) {
            append_tag("CY", cy);
        }
        if (!uy.empty()) {
            append_tag("UY", uy);
        }
        if (barcode_only) {
            append_tag("XB", "barcode_only");
            if (sam_write1(fp_, hdr_, rec_) < 0) {
                throw std::runtime_error("cannot write BAM record");
            }
            ++record_count_;
            return;
        }
        append_tag("GX", gx);
        append_tag("GN", gn);
        // GD: orientation, ';'-separated. Score mode has one 'F'/'R' per GX gene. Ledger modes have
        // one per TX evidence entry, parallel to TX/GX and either GL or GT: D063 three-column bodies
        // use that exact transcript's winning raw exon/body evidence, D066 Ex50pAS uses the exact
        // body/exon model, and legacy two-column bodies retain the gene-majority value.
        append_tag("GD", gd);
        // TX: the read's emitted compatible transcript ids, ';'-separated and deterministically
        // ordered. Exact Ex50pAS can repeat a canonical TX for distinct evidence slots. Production
        // score mode also emits TX for XP/XU provenance, while GX/GD deliberately stay gene-level.
        // Count-mode GX/GD and GL or GT are one entry per TX (GX may therefore repeat a gene).
        if (!tx.empty()) {
            append_tag("TX", tx);
        }
        if (!xr.empty()) {
            append_tag("XR", xr);
        }
        // Production-ledger count modes preserve the exact evidence behind every TX entry. XP and
        // XU use ';' for TX-parallel groups; ordinary modes comma-sort tied values within a group,
        // while exact Ex50pAS emits one path/Parent per group. The strict ledger reader reserves
        // those delimiters in path/Parent identifiers.
        if (!xp.empty()) {
            append_tag("XP", xp);
        }
        if (!xu.empty()) {
            append_tag("XU", xu);
        }
        // GL: non-Ex50 ledger modes only, ';'-separated and parallel to TX/GX/GD -- one S/U call
        // per transcript. D063 calls S from that transcript's exon layer and otherwise U from that
        // transcript's own body layer; legacy two-column bodies retain genomic-span inference.
        // A GENE flagged both S (via one transcript) and U (via another) is velocyto's 'ambiguous' --
        // panCollapse never computes that; a downstream counter groups TX by GX and derives it, then
        // applies the count-mode rule (Gene = spliced-only, GeneFull = any) and the sense/antisense
        // policy. Absent on the Score-mode BAM, so a GL-less BAM counts by XT/GX as before.
        if (!gl.empty()) {
            append_tag("GL", gl);
        }
        // GT: production GeneFull_Ex50pAS only, parallel to TX/GX/GD/XP/XU. E is fully exonic
        // and splice-junction concordant, P is strictly more than half exonic, and B is
        // transcript-body evidence. Direction in GD expands these three strand-neutral tiers to
        // STARsolo's six ordered overlap ranks downstream.
        if (!gt.empty()) {
            append_tag("GT", gt);
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

    bool compact_exact() const { return compact_exact_; }

    // PanCollapse has already applied the compatible-Parent filter, score window, and global
    // library-aware Ex50pAS rank. Retain only its ordered winner Parents and molecule tags.
    void write_compact_exact_record(const std::string& qname, const std::string& cb,
                                    const std::string& ub, const std::string& cy,
                                    const std::string& uy, char gt, char gd,
                                    const std::string& xu) {
        if (!compact_exact_ || xu.empty()) {
            throw std::runtime_error("invalid compact exact-count BAM record");
        }
        if (bam_set1(rec_, qname.size(), qname.c_str(),
                     /*flag=*/BAM_FUNMAP, /*tid=*/-1, /*pos=*/-1, /*mapq=*/0,
                     /*n_cigar=*/0, /*cigar=*/nullptr, /*mtid=*/-1, /*mpos=*/-1,
                     /*isize=*/0, /*l_seq=*/0, /*seq=*/nullptr, /*qual=*/nullptr,
                     /*l_aux=*/0) < 0) {
            throw std::runtime_error("cannot build compact exact-count BAM record");
        }
        append_tag("CB", cb);
        append_tag("CR", cb);
        append_tag("UB", ub);
        append_tag("UR", ub);
        if (!cy.empty()) {
            append_tag("CY", cy);
        }
        if (!uy.empty()) {
            append_tag("UY", uy);
        }
        append_tag("GT", std::string(1, gt));
        append_tag("GD", std::string(1, gd));
        append_tag("XU", xu);
        if (sam_write1(fp_, hdr_, rec_) < 0) {
            throw std::runtime_error("cannot write compact exact-count BAM record");
        }
        ++record_count_;
    }

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
    bool compact_exact_ = false;
    bool finalized_ = false;
};

// Audit-only normalized evidence. This deliberately lives outside the BAM: one row describes the
// read group and each exact-top candidate gets its own row, so parsers never have to unpack a
// variable-length nested tag. The sidecar is opt-in and receives only already-computed state.
class DebugEvidenceWriter {
public:
    explicit DebugEvidenceWriter(const std::filesystem::path& filename)
        : final_path_(filename), temp_path_(filename.string() + ".tmp") {
        if (!final_path_.parent_path().empty()) {
            std::filesystem::create_directories(final_path_.parent_path());
        }
        out_.open(temp_path_);
        if (!out_) {
            throw std::runtime_error("cannot open debug evidence output " +
                                     final_path_.string());
        }
        out_ << "schema_version\tinput_group_ordinal\tinput_name\tqname\trow_type"
                "\tsplice_edge_count\tcandidate_layer\tcanonical_transcript\tgene_id"
                "\tscore\tsplice_concordant\n";
    }

    ~DebugEvidenceWriter() {
        out_.close();
        if (!finalized_) {
            std::error_code ec;
            std::filesystem::remove(temp_path_, ec);
        }
    }

    void write_read(uint64_t ordinal, const std::string& input_name,
                    const std::string& qname, size_t splice_edge_count) {
        write_prefix(ordinal, input_name, qname);
        out_ << "read\t" << splice_edge_count << "\t.\t.\t.\t.\t.\n";
        require_good();
    }

    void write_candidate(uint64_t ordinal, const std::string& input_name,
                         const std::string& qname, const char* layer,
                         const std::string& transcript, const std::string& gene,
                         int64_t score, bool splice_concordant) {
        validate_field(layer, "candidate layer");
        validate_field(transcript, "canonical transcript");
        validate_field(gene, "gene id");
        write_prefix(ordinal, input_name, qname);
        out_ << "candidate\t.\t" << layer << '\t' << transcript << '\t' << gene
             << '\t' << score << '\t' << (splice_concordant ? "true" : "false")
             << '\n';
        require_good();
    }

    void finalize() {
        out_.close();
        if (!out_) {
            throw std::runtime_error("cannot finalize debug evidence output " +
                                     final_path_.string());
        }
        std::filesystem::rename(temp_path_, final_path_);
        finalized_ = true;
    }

private:
    static void validate_field(const std::string& value, const char* label) {
        if (value.empty() || value == "." || value.find_first_of("\t\r\n") != std::string::npos) {
            throw std::runtime_error(std::string("debug evidence ") + label +
                                     " is empty, reserved '.', or contains a TSV delimiter");
        }
    }

    void write_prefix(uint64_t ordinal, const std::string& input_name,
                      const std::string& qname) {
        validate_field(input_name, "input name");
        if (qname != ".") {
            validate_field(qname, "QNAME");
        }
        out_ << "panCollapse-debug-evidence-v1\t" << ordinal << '\t' << input_name
             << '\t' << qname << '\t';
    }

    void require_good() const {
        if (!out_) {
            throw std::runtime_error("cannot write debug evidence output " +
                                     final_path_.string());
        }
    }

    std::filesystem::path final_path_;
    std::filesystem::path temp_path_;
    std::ofstream out_;
    bool finalized_ = false;
};

void write_text_file(const std::filesystem::path& filename, const std::string& contents) {
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("cannot write " + filename.string());
    }
    out << contents;
}

std::string join_sorted(const std::set<std::string>& values, char delimiter) {
    std::string joined;
    for (const std::string& value : values) {
        if (!joined.empty()) {
            joined += delimiter;
        }
        joined += value;
    }
    return joined;
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

struct GroupJob {
    size_t ordinal = 0;
    Group group;
};

// Workers perform read-local computation independently, while every externally visible write is
// serialized by input ordinal.  A guard keeps the turn from its first write through the end of the
// group, so RAD chunks, BAM records, debug rows, warnings, and histograms retain the one-thread
// order.  Debug output can acquire that turn before all computation for a group is complete, which
// intentionally favors audit ordering over debug-mode throughput.  An exception aborts all waiters
// instead of letting a later ordinal deadlock forever.
class OrderedOutputCoordinator {
public:
    explicit OrderedOutputCoordinator(bool enabled) : enabled_(enabled) {}

    class Guard {
    public:
        Guard(OrderedOutputCoordinator& coordinator, size_t ordinal)
            : coordinator_(coordinator), ordinal_(ordinal),
              exceptions_on_entry_(std::uncaught_exceptions()), lock_(coordinator_.mutex_,
                                                                       std::defer_lock) {}

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        void acquire() {
            if (!coordinator_.enabled_ || acquired_) {
                return;
            }
            lock_.lock();
            coordinator_.cv_.wait(lock_, [&] {
                return coordinator_.aborted_ || ordinal_ == coordinator_.next_ordinal_;
            });
            if (coordinator_.aborted_) {
                lock_.unlock();
                throw std::runtime_error("ordered output aborted after a worker failure");
            }
            acquired_ = true;
        }

        ~Guard() {
            if (!acquired_) {
                return;
            }
            const bool unwinding = std::uncaught_exceptions() > exceptions_on_entry_;
            if (unwinding) {
                coordinator_.aborted_ = true;
            } else if (!coordinator_.aborted_) {
                ++coordinator_.next_ordinal_;
            }
            lock_.unlock();
            // During unwinding the worker catch records the original exception and then notifies.
            // Delaying this wake prevents another waiter from racing in with a generic abort error.
            if (!unwinding) {
                coordinator_.cv_.notify_all();
            }
        }

    private:
        OrderedOutputCoordinator& coordinator_;
        size_t ordinal_;
        int exceptions_on_entry_;
        bool acquired_ = false;
        std::unique_lock<std::mutex> lock_;
    };

    void fail(std::exception_ptr failure) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!failure_) {
                failure_ = std::move(failure);
            }
            aborted_ = true;
        }
        cv_.notify_all();
    }

    std::exception_ptr failure() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return failure_;
    }

private:
    friend class Guard;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool enabled_ = true;
    size_t next_ordinal_ = 0;
    bool aborted_ = false;
    std::exception_ptr failure_;
};

int run_convert(int argc, char** argv) {
    const auto invocation_started = std::chrono::steady_clock::now();
    const Options options = parse_options(argc, argv);
    const bool production_identity = !options.path_identity_ledger.empty();
    const std::unordered_set<std::string> strict_allowlisted_parents =
        options.strict_allowlisted_parents.empty()
            ? std::unordered_set<std::string>{}
            : read_strict_allowlisted_parents(options.strict_allowlisted_parents);
    // Production uses one strict, versioned identity ledger. The hst-v1 adapter below is the only
    // route to historical 2/3-column t2g behavior; CLI parsing makes the two routes exclusive.
    std::optional<path_identity::PathIdentityLedger> identity_ledger;
    T2gData t2g;
    BodyT2gData body_t2g;
    if (production_identity) {
        identity_ledger = path_identity::read(
            options.path_identity_ledger,
            options.count_mode == pathtally::CountMode::GeneFullEx50pAS);
        if (!strict_allowlisted_parents.empty()) {
            std::unordered_set<std::string> ledger_parents;
            for (const auto& [path_name, row] : identity_ledger->rows_by_path) {
                static_cast<void>(path_name);
                ledger_parents.insert(row.annotation.unique_parent);
            }
            for (const std::string& parent : strict_allowlisted_parents) {
                if (ledger_parents.count(parent) == 0) {
                    throw std::runtime_error(
                        "strict allowlisted Parent " + parent +
                        " is absent from --path-identity-ledger");
                }
            }
        }
        if (options.count_mode != pathtally::CountMode::Score &&
            !identity_ledger->has_body_layer) {
            throw std::runtime_error(
                "ledger --count-mode gene/genefull/... requires body feature_layer rows");
        }
        for (const auto& entry : identity_ledger->rows_by_path) {
            const std::string& raw_path = entry.first;
            const path_identity::AnnotationIdentity& annotation = entry.second.annotation;
            if (annotation.feature_layer == "exon") {
                t2g.path_transcript.emplace(raw_path, annotation.canonical_transcript);
                t2g.path_unique_parent.emplace(raw_path, annotation.unique_parent);
                t2g.transcript_gene.emplace(annotation.canonical_transcript, annotation.gene_id);
            } else {
                body_t2g.paths.emplace(
                    raw_path,
                    BodyPathTarget{annotation.gene_id, annotation.canonical_transcript});
                body_t2g.path_unique_parent.emplace(raw_path, annotation.unique_parent);
                body_t2g.transcript_gene.emplace(annotation.canonical_transcript,
                                                 annotation.gene_id);
            }
        }
        body_t2g.transcript_specific = true;
        finalize_t2g(t2g);
    } else {
        t2g = read_t2g(options.t2g);
    }
    if (!production_identity && options.count_mode != pathtally::CountMode::Score) {
        body_t2g = read_body_t2g(options.body_t2g);
        if (body_t2g.transcript_specific) {
            for (const auto& [raw_path, body] : body_t2g.paths) {
                if (t2g.path_transcript.count(raw_path) != 0) {
                    throw std::runtime_error("graph path " + raw_path +
                                             " appears in both exon and body t2gs");
                }
            }
            for (const auto& [transcript, gene] : body_t2g.transcript_gene) {
                const auto exon = t2g.transcript_gene.find(transcript);
                if (exon == t2g.transcript_gene.end()) {
                    throw std::runtime_error("body t2g transcript " + transcript +
                                             " is absent from the exon t2g");
                }
                if (exon->second != gene) {
                    throw std::runtime_error("body t2g transcript " + transcript + " maps to gene " +
                                             gene + " but exon t2g maps it to " + exon->second);
                }
            }
        }
    }

    xg::XG graph;
    std::ifstream xg_in(options.xg, std::ios::binary);
    if (!xg_in) {
        throw std::runtime_error("cannot open XG");
    }
    graph.deserialize(xg_in);

    if (production_identity) {
        for (const auto& entry : identity_ledger->rows_by_path) {
            const std::string& path_name = entry.first;
            if (!graph.has_path(path_name)) {
                throw std::runtime_error("path identity ledger vg_path_name " + path_name +
                                         " is absent from the XG");
            }
            const auto path_handle = graph.get_path_handle(path_name);
            const uint64_t graph_length = graph.get_path_length(path_handle);
            if (graph_length != entry.second.vg_path_length) {
                throw std::runtime_error("path identity ledger vg_path_length for " + path_name +
                                         " is " +
                                         std::to_string(entry.second.vg_path_length) +
                                         " but the XG path length is " +
                                         std::to_string(graph_length));
            }
        }
    }

    // Score-mode HST lookup (node -> HST names, cached per node). Ledger-mode lookup is below.
    // Only the one for the active mode is built. Both do the node-id-space validation on first
    // touch.
    std::unordered_map<uint64_t, std::string> hst_path_name;
    std::unordered_map<int64_t, std::vector<std::pair<const std::string*, bool>>> node_hst_cache;
    std::shared_mutex node_hst_cache_mutex;
    pathtally::PathLookup lookup;

    // Ledger-mode state. ledger_path_info covers every exon/body reference path by graph handle.
    // The by-name maps collapse the score tally after each read: exon paths always collapse to a
    // transcript target; legacy two-column bodies collapse to a gene, while D063 three-column
    // bodies collapse to that same canonical transcript target.
    struct PathInfo {
        std::string name;       // raw graph path name, fed to PathLookup
        uint32_t gene_idx = 0;  // gene index (legacy geometry/orientation and BAM header)
        bool is_exon = false;   // spliced exon path vs unspliced body path
    };
    std::unordered_map<uint64_t, PathInfo> ledger_path_info;
    std::unordered_map<std::string, uint64_t> ledger_path_handles_by_name;
    std::unordered_map<std::string, uint32_t> exon_name_transcript;  // exon path name -> transcript target id
    std::unordered_map<std::string, uint32_t> body_name_gene;        // legacy body path -> gene idx
    std::unordered_map<std::string, uint32_t> body_name_transcript;  // D063 body path -> target id
    // Per-transcript spans for the spliced/unspliced classification, built once at load. For each
    // gene, each exon transcript contributes (first_on_body_exon_node_id, last_on_body_exon_node_id,
    // transcript_target_id). At read time a transcript "spans" the read iff its [lo,hi] node-id
    // interval brackets the read's gene-body node-id range; the gene body's nodes run in ascending
    // id order within the gene's contiguous id band, so a node-id interval is the span. See
    // pathtally_ledger.hpp (classify_ledger_group) for how this feeds the classification.
    std::unordered_map<uint32_t, std::vector<pathtally::TranscriptSpan>> transcript_spans;
    // D061 splice-junction concordance. Legacy bodies compare each exon path with its gene's body
    // nodes; D063 compares each canonical transcript's exon paths only with that transcript's own
    // body paths. Either way this maps each splice edge to the target ids that own it.
    pathtally::SpliceEdgeMap splice_edges;
    // A production counting BAM is a typed union: ordinary Gene rows and exact Ex50 rows are
    // computed together from the same GAMP regardless of which downstream count mode selected it.
    // Legacy and score BAMs deliberately keep their marker-absent historical interpretation.
    const bool exact_ex50 = production_identity &&
                           options.count_mode != pathtally::CountMode::Score &&
                           !options.bam_out.empty();
    struct ExactEx50Model {
        const path_identity::PathIdentityRow* exon = nullptr;
        const path_identity::PathIdentityRow* body = nullptr;
        uint64_t exon_path_handle = 0;
        uint64_t body_path_handle = 0;
        uint32_t target_id = 0;
        bool body_exon_same = true;
        std::map<int64_t, uint64_t> resolved_body_positions;
    };
    std::vector<ExactEx50Model> exact_ex50_models;
    std::unordered_map<uint64_t, std::vector<size_t>> exact_ex50_models_by_body_path;
    // A body row is retained in the ledger even when it cannot support exact base geometry.  The
    // Parent then remains countable from its exon evidence, but never contributes an Ex50 body
    // model.  This is deliberately per Parent: a cyclic body path must not disable clean loci.
    std::set<std::string> body_unresolvable_exon_parents;
    std::set<std::string> body_unresolvable_paths;
    // Geometry is grouped by canonical target, so cache the targets that contain at least one
    // degraded Parent while the Parent-level failure is discovered.  Re-deriving this fact by
    // scanning every exon path inside record_geometry makes initialization quadratic in the
    // number of production paths on chromosome-scale graphs.
    std::unordered_set<uint32_t> body_unresolvable_targets;
    // Per-node ledger cache. ref_paths feeds the score tally (tally_read_group_into, reused verbatim
    // from score mode): every reference path crossing the node, exon or gene-body, un-collapsed, so
    // a read's per-reference score here is exactly what score mode would compute for that
    // reference. gene_orient/body_genes preserve the D060 two-column behavior. D063 derives GD
    // from the same max-scoring raw exon/body tallies used for each transcript's S/U call, below,
    // so tied aliases combine orientation evidence deterministically instead of depending on graph
    // path iteration order.
    struct NodeLedger {
        uint64_t node_length = 0;
        std::vector<std::pair<const std::string*, bool>> ref_paths;
        struct LegacyMetadata {
            std::vector<std::pair<uint32_t, bool>> gene_orient;
            std::vector<uint32_t> body_genes;
        };
        std::optional<LegacyMetadata> legacy;
        struct ExactStep {
            uint64_t path_handle = 0;
            bool path_is_reverse = false;
            uint64_t path_position = 0;
        };
        std::vector<ExactStep> exact_exon_steps;
        std::vector<ExactStep> exact_body_steps;
    };
    std::unordered_map<int64_t, NodeLedger> node_ledger_cache;
    std::shared_mutex node_ledger_cache_mutex;
    std::function<const NodeLedger&(int64_t)> node_ledger_of;
    pathtally::PathLookup ledger_lookup;

    if (options.count_mode == pathtally::CountMode::Score) {
        graph.for_each_path_handle([&](const handlegraph::path_handle_t& path) {
            std::string name = graph.get_path_name(path);
            if (resolve_graph_transcript(t2g, name, false) != nullptr) {
                hst_path_name.emplace(handlegraph::as_integer(path), std::move(name));
            }
        });
        lookup = [&](int64_t node_id, const std::function<void(const std::string&, bool)>& emit) {
            if (options.threads > 1) {
                std::shared_lock<std::shared_mutex> lock(node_hst_cache_mutex);
                const auto cached = node_hst_cache.find(node_id);
                if (cached != node_hst_cache.end()) {
                    for (const auto& [name_ptr, path_is_reverse] : cached->second) {
                        emit(*name_ptr, path_is_reverse);
                    }
                    return;
                }
            }
            std::unique_lock<std::shared_mutex> lock(node_hst_cache_mutex, std::defer_lock);
            if (options.threads > 1) {
                lock.lock();
            }
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
        // Legacy geometry is grouped by gene. D063 geometry is grouped by canonical transcript.
        std::unordered_map<uint32_t, std::vector<handlegraph::path_handle_t>> gene_exon_paths;
        std::unordered_map<uint32_t, std::vector<handlegraph::path_handle_t>> gene_body_paths;
        std::unordered_map<uint32_t, std::vector<handlegraph::path_handle_t>> transcript_exon_paths;
        std::unordered_map<uint32_t, std::vector<handlegraph::path_handle_t>> transcript_body_paths;
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
        auto record_exon_path = [&](const handlegraph::path_handle_t& path,
                                    const std::string& name, const std::string& transcript) {
            const std::string& gene = t2g.transcript_gene.at(transcript);
            const uint32_t gene_idx = gene_index_for(gene);
            const uint32_t target_id = t2g.target_ids.at(transcript);
            ledger_path_info.emplace(handlegraph::as_integer(path),
                                     PathInfo{name, gene_idx, true});
            exon_name_transcript.emplace(name, target_id);
            gene_exon_paths[gene_idx].push_back(path);
            transcript_exon_paths[target_id].push_back(path);
        };

        graph.for_each_path_handle([&](const handlegraph::path_handle_t& path) {
            const std::string name = graph.get_path_name(path);
            // Exact exon rows win first. Then exact body rows win over the legacy exon bare-name
            // fallback, so an explicitly named transcript-body path cannot be mistaken for an exon.
            const std::string* transcript = resolve_graph_transcript(t2g, name, false);
            if (transcript != nullptr) {
                record_exon_path(path, name, *transcript);
                ledger_path_handles_by_name.emplace(name, handlegraph::as_integer(path));
                return;
            }
            const BodyPathTarget* body = resolve_body_graph_path(body_t2g, name);
            if (body != nullptr) {
                const uint32_t gene_idx = gene_index_for(body->gene);
                if (body_t2g.transcript_specific) {
                    const uint32_t target_id = t2g.target_ids.at(body->transcript);
                    ledger_path_info.emplace(handlegraph::as_integer(path),
                                             PathInfo{name, gene_idx, false});
                    body_name_transcript.emplace(name, target_id);
                    transcript_body_paths[target_id].push_back(path);
                } else {
                    ledger_path_info.emplace(handlegraph::as_integer(path),
                                             PathInfo{name, gene_idx, false});
                    body_name_gene.emplace(name, gene_idx);
                    gene_body_paths[gene_idx].push_back(path);
                }
                ledger_path_handles_by_name.emplace(name, handlegraph::as_integer(path));
                return;
            }
            if (!production_identity) {
                transcript = resolve_graph_transcript(t2g, name, true);
                if (transcript != nullptr) {
                    record_exon_path(path, name, *transcript);
                }
            }
        });

        if (exact_ex50) {
            std::map<std::string, std::vector<const path_identity::PathIdentityRow*>>
                exon_rows_by_parent;
            std::map<std::string, std::vector<const path_identity::PathIdentityRow*>>
                body_rows_by_exon_parent;
            for (const auto& [path_name, row] : identity_ledger->rows_by_path) {
                if (row.annotation.feature_layer == "exon") {
                    exon_rows_by_parent[row.annotation.unique_parent].push_back(&row);
                } else {
                    body_rows_by_exon_parent[row.annotation.exon_unique_parent].push_back(&row);
                }
            }
            for (const auto& [exon_parent, exon_rows] : exon_rows_by_parent) {
                const auto bodies = body_rows_by_exon_parent.find(exon_parent);
                if (bodies == body_rows_by_exon_parent.end() || bodies->second.empty()) {
                    throw std::runtime_error(
                        "genefull_ex50pas requires a linked body path for exon Parent " +
                        exon_parent);
                }
                bool parent_body_unresolvable = false;
                // Keep resolved geometry local to this exon Parent.  Positions only distinguish
                // repeated body occurrences; ordinary exonic nodes have one unambiguous body
                // occurrence and need no per-node storage.
                std::vector<ExactEx50Model> parent_models;
                for (const path_identity::PathIdentityRow* exon : exon_rows) {
                    const handlegraph::path_handle_t exon_path =
                        graph.get_path_handle(exon->vg_path_name);
                    for (const path_identity::PathIdentityRow* body : bodies->second) {
                        const handlegraph::path_handle_t body_path =
                            graph.get_path_handle(body->vg_path_name);
                        std::map<int64_t, std::vector<std::pair<bool, uint64_t>>> body_occurrences;
                        graph.for_each_step_in_path(
                            body_path, [&](const handlegraph::step_handle_t& step) {
                                const handlegraph::handle_t handle = graph.get_handle_of_step(step);
                                body_occurrences[graph.get_id(handle)].emplace_back(
                                    graph.get_is_reverse(handle), graph.get_position_of_step(step));
                            });

                        // A repeated node is safe only when the exon path fixes one body occurrence.
                        // The body/exon orientation relation is global for a model, so test both
                        // possible relations and retain it only when exactly one relation gives one
                        // oriented occurrence for every shared exon step.  This chooses a uniquely
                        // positioned occurrence; multiple consistent positions remain ambiguous.
                        bool repeated_exonic_node = false;
                        int compatible_relations = 0;
                        bool selected_relation = true;
                        std::map<int64_t, uint64_t> selected_positions;
                        for (const bool same_orientation : {false, true}) {
                            bool compatible = true;
                            bool shared = false;
                            graph.for_each_step_in_path(
                                exon_path, [&](const handlegraph::step_handle_t& step) {
                                    const handlegraph::handle_t handle = graph.get_handle_of_step(step);
                                    const auto occurrences = body_occurrences.find(graph.get_id(handle));
                                    if (occurrences == body_occurrences.end()) {
                                        return true;
                                    }
                                    shared = true;
                                    if (occurrences->second.size() > 1) {
                                        repeated_exonic_node = true;
                                    }
                                    const bool exon_reverse = graph.get_is_reverse(handle);
                                    const size_t matching = static_cast<size_t>(std::count_if(
                                        occurrences->second.begin(), occurrences->second.end(),
                                        [&](const auto& occurrence) {
                                            return (exon_reverse == occurrence.first) == same_orientation;
                                        }));
                                    if (matching != 1) {
                                        compatible = false;
                                    }
                                    return true;
                                });
                            if (shared && compatible) {
                                ++compatible_relations;
                                selected_relation = same_orientation;
                                selected_positions.clear();
                                graph.for_each_step_in_path(exon_path, [&](const handlegraph::step_handle_t& step) {
                                    const handlegraph::handle_t handle = graph.get_handle_of_step(step);
                                    const int64_t id = graph.get_id(handle);
                                    const auto occurrences = body_occurrences.find(id);
                                    if (occurrences != body_occurrences.end()) {
                                        const bool exon_reverse = graph.get_is_reverse(handle);
                                        for (const auto& occurrence : occurrences->second) {
                                            if ((exon_reverse == occurrence.first) == same_orientation) {
                                                if (occurrences->second.size() > 1) {
                                                    selected_positions[id] = occurrence.second;
                                                }
                                            }
                                        }
                                    }
                                    return true;
                                });
                            }
                        }
                        if (repeated_exonic_node && compatible_relations != 1) {
                            if (exon->annotation.gene_id != body->annotation.gene_id) {
                                throw std::runtime_error(
                                    "refusing body-path degradation because linked Parent " +
                                    exon_parent + " changes counted gene from " +
                                    exon->annotation.gene_id + " to " + body->annotation.gene_id);
                            }
                            parent_body_unresolvable = true;
                            body_unresolvable_paths.insert(body->vg_path_name);
                            body_unresolvable_targets.insert(
                                t2g.target_ids.at(exon->annotation.canonical_transcript));
                        } else if (compatible_relations == 0) {
                            throw std::runtime_error(
                                "genefull_ex50pas cannot establish body/exon orientation for linked paths " +
                                body->vg_path_name + " and " + exon->vg_path_name);
                        } else {
                            if (exon->annotation.gene_id != body->annotation.gene_id) {
                                throw std::runtime_error(
                                    "path identity ledger body Parent " +
                                    body->annotation.unique_parent +
                                    " changes counted gene from " + exon->annotation.gene_id +
                                    " to " + body->annotation.gene_id);
                            }
                            parent_models.push_back(
                                {exon, body, handlegraph::as_integer(exon_path),
                                 handlegraph::as_integer(body_path),
                                 t2g.target_ids.at(exon->annotation.canonical_transcript),
                                 selected_relation, std::move(selected_positions)});
                        }
                    }
                }
                if (parent_body_unresolvable) {
                    body_unresolvable_exon_parents.insert(exon_parent);
                    continue;
                }
                for (ExactEx50Model& model : parent_models) {
                    const size_t model_id = exact_ex50_models.size();
                    exact_ex50_models_by_body_path[model.body_path_handle].push_back(model_id);
                    exact_ex50_models.push_back(std::move(model));
                }
            }

            // This remains a fail-closed completeness invariant for accidental ledger omissions.
            // A Parent marked above is an observed body-geometry failure, not a missing body row;
            // it is accepted only because its exon and body identities proved the same counted gene.
            std::set<std::string> exon_parents;
            std::set<std::string> body_linked_exon_parents;
            for (const auto& [path_name, row] : identity_ledger->rows_by_path) {
                static_cast<void>(path_name);
                if (row.annotation.feature_layer == "exon") {
                    exon_parents.insert(row.annotation.unique_parent);
                } else {
                    body_linked_exon_parents.insert(row.annotation.exon_unique_parent);
                }
            }
            body_linked_exon_parents.insert(body_unresolvable_exon_parents.begin(),
                                            body_unresolvable_exon_parents.end());
            std::vector<std::string> missing;
            std::set_difference(exon_parents.begin(), exon_parents.end(),
                                body_linked_exon_parents.begin(), body_linked_exon_parents.end(),
                                std::back_inserter(missing));
            if (!missing.empty()) {
                throw std::runtime_error(
                    "production typed-union count-mode BAM requires at least one linked body "
                    "row for every exon Parent; missing " +
                    std::to_string(missing.size()) + " exon Parent(s), first: " +
                    missing.front());
            }
        }

        size_t d63_splice_target_edges_evaluated = 0;
        size_t d63_splice_target_edges_owned = 0;
        size_t d63_splice_target_edges_fragment_only = 0;
        size_t d63_splice_target_edges_adjacent_vetoed = 0;

        // Shared geometry worker. Legacy D060 records exon spans against a pooled gene body. D063
        // omits spans and derives splice ownership by comparing one canonical transcript's exon
        // paths only with that transcript's own body paths.
        auto record_geometry = [&](const std::vector<handlegraph::path_handle_t>& exon_paths,
                                   const std::vector<handlegraph::path_handle_t>& body_paths,
                                   const std::optional<uint32_t>& legacy_gene_idx) {
            if (!legacy_gene_idx.has_value()) {
                using Edge = std::pair<int64_t, int64_t>;
                std::unordered_set<Edge, pathtally::NodePairHash> exon_edges;
                std::optional<uint32_t> target_id;
                for (const handlegraph::path_handle_t& exon_path : exon_paths) {
                    const uint32_t path_target =
                        exon_name_transcript.at(graph.get_path_name(exon_path));
                    if (!target_id.has_value()) {
                        target_id = path_target;
                    } else if (*target_id != path_target) {
                        throw std::runtime_error(
                            "internal transcript-body geometry mixed canonical targets");
                    }
                    std::vector<int64_t> node_ids;
                    graph.for_each_step_in_path(
                        exon_path, [&](const handlegraph::step_handle_t& step) {
                            node_ids.push_back(graph.get_id(graph.get_handle_of_step(step)));
                    });
                    for (size_t i = 0; i + 1 < node_ids.size(); ++i) {
                        if (node_ids[i] != node_ids[i + 1]) {
                            exon_edges.insert(
                                pathtally::undirected_node_pair(node_ids[i], node_ids[i + 1]));
                        }
                    }
                }
                if (!target_id.has_value()) {
                    return;
                }

                // A degraded Parent cannot contribute body geometry, but its exon path remains
                // valid evidence.  Preserve its own exon edges only when no clean sibling body
                // for this canonical target remains; otherwise use only clean body geometry.
                const bool degraded_parent =
                    production_identity && body_unresolvable_targets.count(*target_id) != 0;
                // For the fragment-only audit, record whether both endpoints ever coexist on one
                // raw body path. Index each candidate edge by its lower endpoint so each body path
                // only probes candidates incident on nodes it actually contains.
                std::unordered_map<int64_t, std::vector<Edge>> exon_edges_by_first;
                for (const Edge& edge : exon_edges) {
                    exon_edges_by_first[edge.first].push_back(edge);
                }

                pathtally::BodyGeometryIndex body_geometry;
                std::unordered_set<Edge, pathtally::NodePairHash> same_path_endpoint_edges;
                bool has_usable_body_path = false;
                for (const handlegraph::path_handle_t& body_path : body_paths) {
                    if (production_identity) {
                        const auto& annotation = identity_ledger->rows_by_path.at(
                            graph.get_path_name(body_path)).annotation;
                        if (body_unresolvable_exon_parents.count(
                                annotation.exon_unique_parent) != 0) {
                            continue;
                        }
                    }
                    has_usable_body_path = true;
                    std::vector<int64_t> path_node_ids;
                    graph.for_each_step_in_path(
                        body_path, [&](const handlegraph::step_handle_t& step) {
                            path_node_ids.push_back(graph.get_id(graph.get_handle_of_step(step)));
                        });
                    pathtally::add_body_path(body_geometry, path_node_ids);
                    if (same_path_endpoint_edges.size() == exon_edges.size()) {
                        continue;  // all candidates already have same-path support; only union/adjacency remains
                    }
                    std::unordered_set<int64_t> path_nodes(path_node_ids.begin(),
                                                           path_node_ids.end());
                    for (const int64_t node_id : path_nodes) {
                        const auto candidates = exon_edges_by_first.find(node_id);
                        if (candidates == exon_edges_by_first.end()) {
                            continue;
                        }
                        for (const Edge& edge : candidates->second) {
                            if (path_nodes.count(edge.second) != 0) {
                                same_path_endpoint_edges.insert(edge);
                            }
                        }
                    }
                }

                if (degraded_parent && !has_usable_body_path) {
                    for (const Edge& edge : exon_edges) {
                        ++d63_splice_target_edges_evaluated;
                        ++d63_splice_target_edges_owned;
                        splice_edges[edge].insert(*target_id);
                    }
                    return;
                }

                for (const Edge& edge : exon_edges) {
                    ++d63_splice_target_edges_evaluated;
                    if (body_geometry.adjacent_edges.count(edge) != 0) {
                        ++d63_splice_target_edges_adjacent_vetoed;
                        continue;
                    }
                    if (!pathtally::body_geometry_owns_splice_edge(
                            body_geometry, edge.first, edge.second)) {
                        continue;
                    }
                    ++d63_splice_target_edges_owned;
                    if (same_path_endpoint_edges.count(edge) == 0) {
                        ++d63_splice_target_edges_fragment_only;
                    }
                    splice_edges[edge].insert(*target_id);
                }
                return;
            }

            // Legacy D060 deliberately retains its historical numeric span/edge behavior.
            std::unordered_set<int64_t> body_nodes;
            for (const handlegraph::path_handle_t& body_path : body_paths) {
                graph.for_each_step_in_path(body_path, [&](const handlegraph::step_handle_t& step) {
                    body_nodes.insert(graph.get_id(graph.get_handle_of_step(step)));
                });
            }
            std::vector<int64_t> body_sorted(body_nodes.begin(), body_nodes.end());
            std::sort(body_sorted.begin(), body_sorted.end());

            std::vector<pathtally::TranscriptSpan>& spans = transcript_spans[*legacy_gene_idx];
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
                    const auto edge = std::make_pair(edge_lo, edge_hi);
                    const auto between =
                        std::upper_bound(body_sorted.begin(), body_sorted.end(), edge_lo);
                    const bool is_splice = between != body_sorted.end() && *between < edge_hi;
                    if (is_splice) {
                        splice_edges[edge].insert(target_id);
                    }
                }
            }
        };

        if (body_t2g.transcript_specific) {
            for (const auto& [target_id, exon_paths] : transcript_exon_paths) {
                const auto body = transcript_body_paths.find(target_id);
                const std::vector<handlegraph::path_handle_t> no_body;
                record_geometry(exon_paths,
                                body == transcript_body_paths.end() ? no_body : body->second,
                                std::nullopt);
            }
            std::cerr << "panCollapse: transcript-body splice geometry: evaluated_target_edges="
                      << d63_splice_target_edges_evaluated
                      << " owned_target_edges=" << d63_splice_target_edges_owned
                      << " fragment_only_target_edges="
                      << d63_splice_target_edges_fragment_only
                      << " adjacent_vetoed_target_edges="
                      << d63_splice_target_edges_adjacent_vetoed
                      << " body_paths_degraded=" << body_unresolvable_paths.size()
                      << " body_path_degrade_reason=repeated_exonic_node" << '\n';
            if (!options.debug_evidence_out.empty()) {
                for (const std::string& path : body_unresolvable_paths) {
                    std::cerr << "panCollapse: debug: degraded body path " << path
                              << " (repeated_exonic_node)\n";
                }
            }
        } else {
            for (const auto& [gene_idx, exon_paths] : gene_exon_paths) {
                const auto body = gene_body_paths.find(gene_idx);
                const std::vector<handlegraph::path_handle_t> no_body;
                record_geometry(exon_paths, body == gene_body_paths.end() ? no_body : body->second,
                                gene_idx);
            }
        }

        node_ledger_of = [&](int64_t node_id) -> const NodeLedger& {
            if (options.threads > 1) {
                std::shared_lock<std::shared_mutex> lock(node_ledger_cache_mutex);
                const auto cached = node_ledger_cache.find(node_id);
                if (cached != node_ledger_cache.end()) {
                    return cached->second;
                }
            }
            std::unique_lock<std::shared_mutex> lock(node_ledger_cache_mutex, std::defer_lock);
            if (options.threads > 1) {
                lock.lock();
            }
            auto cached = node_ledger_cache.find(node_id);
            if (cached == node_ledger_cache.end()) {
                if (!graph.has_node(node_id)) {
                    throw std::runtime_error(
                        "GAMP/xg node-id-space mismatch: node " + std::to_string(node_id) +
                        " is absent from the graph; the GAMP was likely aligned to a different graph");
                }
                NodeLedger nl;
                nl.node_length = graph.get_length(graph.get_handle(node_id, false));
                // These two metadata views serve only legacy two-column body classification.
                // Transcript-specific bodies derive orientation from the winning raw path and do
                // not use gene-wide body spans.
                std::optional<std::map<uint32_t, bool>> gene_orient_here;
                std::optional<std::set<uint32_t>> body_genes_here;
                if (!body_t2g.transcript_specific) {
                    gene_orient_here.emplace();
                    body_genes_here.emplace();
                }
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
                    if (exact_ex50) {
                        NodeLedger::ExactStep exact_step{
                            path_handle, path_is_reverse,
                            graph.get_position_of_step(step)};
                        if (it->second.is_exon) {
                            nl.exact_exon_steps.push_back(exact_step);
                        } else {
                            nl.exact_body_steps.push_back(exact_step);
                        }
                    }
                    if (!body_t2g.transcript_specific) {
                        (*gene_orient_here)[it->second.gene_idx] = path_is_reverse;
                        if (!it->second.is_exon) {
                            body_genes_here->insert(it->second.gene_idx);
                        }
                    }
                    return true;
                });
                if (!body_t2g.transcript_specific) {
                    nl.legacy.emplace();
                    for (const auto& [gene_idx, is_reverse] : *gene_orient_here) {
                        nl.legacy->gene_orient.emplace_back(gene_idx, is_reverse);
                    }
                    nl.legacy->body_genes.assign(body_genes_here->begin(), body_genes_here->end());
                }
                auto by_path_handle = [](const NodeLedger::ExactStep& a,
                                         const NodeLedger::ExactStep& b) {
                    return a.path_handle < b.path_handle;
                };
                std::sort(nl.exact_exon_steps.begin(), nl.exact_exon_steps.end(),
                          by_path_handle);
                std::sort(nl.exact_body_steps.begin(), nl.exact_body_steps.end(),
                          by_path_handle);
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

    // Production GeneFull_Ex50pAS is a BAM-only evidence producer. Unlike the historical S/U
    // classifier above, this walks the MultipathAlignment DAG without enumerating its complete
    // traversals. A state exists only while one exact body path can contain that traversal, and
    // carries exact reference-base and exon-overlap totals plus junction concordance for one linked
    // exon path. Distinct exact paths/Parents remain distinct evidence slots; there is deliberately
    // no Parent -> canonical collapse before count_cr applies STAR's six-rank priority.
    struct ExactEx50Evidence {
        uint32_t target_id = 0;
        std::string locus_parent;
        std::string path;
        std::string parent;
        char direction = 'F';
        pathtally::Ex50Tier tier = pathtally::Ex50Tier::Body;
        int64_t score = std::numeric_limits<int64_t>::min();
    };
    auto exact_evidence_less = [](const ExactEx50Evidence& a, const ExactEx50Evidence& b) {
        auto tier_rank = [](pathtally::Ex50Tier tier) {
            switch (tier) {
            case pathtally::Ex50Tier::FullyExonic:
                return 0;
            case pathtally::Ex50Tier::ExonicMajority:
                return 1;
            case pathtally::Ex50Tier::Body:
                return 2;
            }
            return 3;
        };
        return std::make_tuple(a.target_id, a.locus_parent, a.direction, tier_rank(a.tier),
                               a.path, a.parent) <
               std::make_tuple(b.target_id, b.locus_parent, b.direction, tier_rank(b.tier),
                               b.path, b.parent);
    };
    using ExactEx50EvidenceSet =
        std::set<ExactEx50Evidence, decltype(exact_evidence_less)>;
    auto retain_best_exact_evidence = [](ExactEx50EvidenceSet& evidence,
                                         ExactEx50Evidence candidate) {
        const auto existing = evidence.find(candidate);
        if (existing == evidence.end()) {
            evidence.insert(std::move(candidate));
        } else if (candidate.score > existing->score) {
            evidence.erase(existing);
            evidence.insert(std::move(candidate));
        }
    };

    using ExactEdge = std::tuple<int64_t, bool, int64_t, bool>;
    std::map<uint64_t, std::set<ExactEdge>> exact_exon_edges;
    std::shared_mutex exact_exon_edges_mutex;

    auto exact_step_range = [](const std::vector<NodeLedger::ExactStep>& steps,
                               uint64_t path_handle) {
        const auto first = std::lower_bound(
            steps.begin(), steps.end(), path_handle,
            [](const NodeLedger::ExactStep& step, uint64_t wanted) {
                return step.path_handle < wanted;
            });
        const auto last = std::upper_bound(
            first, steps.end(), path_handle,
            [](uint64_t wanted, const NodeLedger::ExactStep& step) {
                return wanted < step.path_handle;
            });
        return std::make_pair(first, last);
    };

    auto exon_edges_for = [&](const ExactEx50Model& model) -> const std::set<ExactEdge>& {
        if (options.threads > 1) {
            std::shared_lock<std::shared_mutex> lock(exact_exon_edges_mutex);
            const auto cached = exact_exon_edges.find(model.exon_path_handle);
            if (cached != exact_exon_edges.end()) {
                return cached->second;
            }
        }
        std::unique_lock<std::shared_mutex> lock(exact_exon_edges_mutex, std::defer_lock);
        if (options.threads > 1) {
            lock.lock();
        }
        auto cached = exact_exon_edges.find(model.exon_path_handle);
        if (cached == exact_exon_edges.end()) {
            auto& edges = exact_exon_edges[model.exon_path_handle];
            const handlegraph::path_handle_t exon_path =
                graph.get_path_handle(model.exon->vg_path_name);
            std::optional<std::pair<int64_t, bool>> previous;
            graph.for_each_step_in_path(
                exon_path, [&](const handlegraph::step_handle_t& step) {
                    const handlegraph::handle_t handle = graph.get_handle_of_step(step);
                    const std::pair<int64_t, bool> current{
                        graph.get_id(handle), graph.get_is_reverse(handle)};
                    if (previous.has_value()) {
                        edges.emplace(previous->first, previous->second, current.first,
                                      current.second);
                        // The reverse traversal uses reversed node order and flipped handles.
                        edges.emplace(current.first, !current.second, previous->first,
                                      !previous->second);
                    }
                    previous = current;
                });
            cached = exact_exon_edges.find(model.exon_path_handle);
        }
        return cached->second;
    };

    auto body_exon_orientation_same = [&](size_t model_id) {
        return exact_ex50_models.at(model_id).body_exon_same;
    };

    std::atomic<uint64_t> exact_unstarted_model_candidates_before_prefilter{0};
    std::atomic<uint64_t> exact_unstarted_model_candidates_after_prefilter{0};

    auto exact_ex50_evidence_for =
        [&](const vg::MultipathAlignment& alignment) -> ExactEx50EvidenceSet {
        ExactEx50EvidenceSet evidence(exact_evidence_less);
        if (!exact_ex50 || alignment.subpath_size() == 0) {
            return evidence;
        }

        struct State {
            size_t model_id = std::numeric_limits<size_t>::max();
            bool body_forward = true;
            uint64_t lo = 0;
            uint64_t hi = 0;
            uint64_t alignment_bases = 0;
            uint64_t exon_overlap = 0;
            int64_t last_node = 0;
            bool last_mapping_reverse = false;
            bool last_at_node_end = false;
            bool splice_concordant = true;
            int64_t score = 0;
        };
        constexpr size_t kUnstarted = std::numeric_limits<size_t>::max();
        struct StateKey {
            size_t model_id;
            bool body_forward;
            uint64_t lo;
            uint64_t hi;
            uint64_t alignment_bases;
            uint64_t exon_overlap;
            int64_t last_node;
            bool last_mapping_reverse;
            bool last_at_node_end;
            bool splice_concordant;

            explicit StateKey(const State& state)
                : model_id(state.model_id), body_forward(state.body_forward), lo(state.lo),
                  hi(state.hi), alignment_bases(state.alignment_bases),
                  exon_overlap(state.exon_overlap), last_node(state.last_node),
                  last_mapping_reverse(state.last_mapping_reverse),
                  last_at_node_end(state.last_at_node_end),
                  splice_concordant(state.splice_concordant) {}

            bool operator==(const StateKey& other) const = default;
        };
        struct StateKeyHash {
            size_t operator()(const StateKey& key) const {
                size_t hash = 0;
                auto combine = [&](const auto& value) {
                    const size_t value_hash = std::hash<std::decay_t<decltype(value)>>{}(value);
                    hash ^= value_hash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                };
                combine(key.model_id);
                combine(key.body_forward);
                combine(key.lo);
                combine(key.hi);
                combine(key.alignment_bases);
                combine(key.exon_overlap);
                combine(key.last_node);
                combine(key.last_mapping_reverse);
                combine(key.last_at_node_end);
                combine(key.splice_concordant);
                return hash;
            }
        };
        std::unordered_map<StateKey, size_t, StateKeyHash> state_index;
        auto deduplicate_states = [&](std::vector<State>& states) {
            state_index.clear();
            state_index.reserve(states.size());
            std::vector<State> retained;
            retained.reserve(states.size());
            for (State& state : states) {
                const auto [it, inserted] = state_index.try_emplace(StateKey(state), retained.size());
                if (inserted) {
                    retained.push_back(std::move(state));
                } else if (state.score > retained[it->second].score) {
                    retained[it->second].score = state.score;
                }
            }
            states = std::move(retained);
        };

        auto mapping_reference_bases = [](const vg::Mapping& mapping) {
            uint64_t bases = 0;
            for (const vg::Edit& edit : mapping.edit()) {
                bases += static_cast<uint64_t>(edit.from_length());
            }
            return bases;
        };

        auto project_mapping = [&](const State& prior, size_t model_id,
                                   const NodeLedger& node,
                                   const NodeLedger::ExactStep& body_step,
                                   const vg::Mapping& mapping, uint64_t reference_bases,
                                   bool initialize) -> std::optional<State> {
            const ExactEx50Model& model = exact_ex50_models.at(model_id);
            const int64_t node_id = mapping.position().node_id();
            const uint64_t node_length = node.node_length;
            const uint64_t offset = static_cast<uint64_t>(mapping.position().offset());
            if (offset > node_length || reference_bases > node_length - offset) {
                throw std::runtime_error(
                    "GAMP mapping offset/reference length exceeds graph node " +
                    std::to_string(node_id));
            }

            const bool mapping_reverse = mapping.position().is_reverse();
            const bool body_forward = mapping_reverse == body_step.path_is_reverse;
            const uint64_t step_start = body_step.path_position;
            const uint64_t lo = body_forward
                ? step_start + offset
                : step_start + node_length - offset - reference_bases;
            const uint64_t hi = lo + reference_bases;

            const auto exon_occurrences =
                exact_step_range(node.exact_exon_steps, model.exon_path_handle);
            const bool exonic = exon_occurrences.first != exon_occurrences.second;
            if (exonic) {
                const auto chosen = model.resolved_body_positions.find(node_id);
                if (chosen != model.resolved_body_positions.end() &&
                    body_step.path_position != chosen->second) {
                    return std::nullopt;
                }
            }

            State next = prior;
            if (initialize) {
                next = {};
                next.model_id = model_id;
                next.body_forward = body_forward;
                next.score = prior.score;
            } else {
                if (prior.model_id != model_id || prior.body_forward != body_forward) {
                    return std::nullopt;
                }
                uint64_t gap = 0;
                if (body_forward) {
                    if (lo < prior.hi) {
                        return std::nullopt;
                    }
                    gap = lo - prior.hi;
                } else {
                    if (hi > prior.lo) {
                        return std::nullopt;
                    }
                    gap = prior.lo - hi;
                }
                if (gap > 0) {
                    const ExactEdge edge{prior.last_node, prior.last_mapping_reverse, node_id,
                                         mapping_reverse};
                    const bool boundary_exact =
                        prior.last_at_node_end && offset == 0;
                    next.splice_concordant =
                        next.splice_concordant && boundary_exact &&
                        exon_edges_for(model).count(edge) != 0;
                }
            }

            next.lo = lo;
            next.hi = hi;
            next.alignment_bases += reference_bases;
            if (exonic) {
                next.exon_overlap += reference_bases;
            }
            next.last_node = node_id;
            next.last_mapping_reverse = mapping_reverse;
            next.last_at_node_end = offset + reference_bases == node_length;
            return next;
        };

        const size_t subpath_count =
            static_cast<size_t>(alignment.subpath_size());
        std::vector<std::vector<State>> incoming(subpath_count);
        std::vector<char> is_source(subpath_count, 0);
        if (alignment.start_size() > 0) {
            for (const uint32_t start : alignment.start()) {
                if (start >= subpath_count) {
                    throw std::runtime_error(
                        "GAMP MultipathAlignment start index is out of range");
                }
                is_source[start] = 1;
            }
        } else {
            std::vector<char> has_incoming(subpath_count, 0);
            for (size_t i = 0; i < subpath_count; ++i) {
                const vg::Subpath& subpath =
                    alignment.subpath(static_cast<int>(i));
                for (const uint32_t next : subpath.next()) {
                    if (next >= subpath_count) {
                        throw std::runtime_error(
                            "GAMP MultipathAlignment next index is out of range");
                    }
                    has_incoming[next] = 1;
                }
                for (const vg::Connection& connection : subpath.connection()) {
                    if (connection.next() >= subpath_count) {
                        throw std::runtime_error(
                            "GAMP MultipathAlignment connection index is out of range");
                    }
                    has_incoming[connection.next()] = 1;
                }
            }
            for (size_t i = 0; i < subpath_count; ++i) {
                is_source[i] = !has_incoming[i];
            }
        }
        for (size_t i = 0; i < subpath_count; ++i) {
            if (is_source[i]) {
                incoming[i].push_back(State{});
                incoming[i].back().model_id = kUnstarted;
            }
        }

        for (size_t subpath_index = 0; subpath_index < subpath_count;
             ++subpath_index) {
            std::vector<State> states = std::move(incoming[subpath_index]);
            deduplicate_states(states);
            const vg::Subpath& subpath =
                alignment.subpath(static_cast<int>(subpath_index));
            for (State& state : states) {
                state.score += subpath.score();
            }

            // kUnstarted cannot survive a reference-consuming mapping: that mapping replaces it
            // with projected states (or removes it).  Therefore every model it can initialize in
            // this subpath must have a body-path step at every nonzero-reference mapping.  This
            // is a necessary-only prefilter; position, orientation, and contiguity remain checked
            // by project_mapping below.
            const bool has_unstarted = std::any_of(
                states.begin(), states.end(), [&](const State& state) {
                    return state.model_id == kUnstarted;
                });
            std::optional<std::unordered_set<size_t>> unstarted_model_filter;
            if (has_unstarted) {
                bool first_reference_mapping = true;
                for (const vg::Mapping& mapping : subpath.path().mapping()) {
                    if (mapping_reference_bases(mapping) == 0) {
                        continue;
                    }
                    const NodeLedger& node = node_ledger_of(mapping.position().node_id());
                    std::unordered_set<size_t> mapping_models;
                    for (const NodeLedger::ExactStep& body_step : node.exact_body_steps) {
                        const auto models = exact_ex50_models_by_body_path.find(
                            body_step.path_handle);
                        if (models != exact_ex50_models_by_body_path.end()) {
                            mapping_models.insert(models->second.begin(), models->second.end());
                        }
                    }
                    if (first_reference_mapping) {
                        exact_unstarted_model_candidates_before_prefilter.fetch_add(
                            mapping_models.size(), std::memory_order_relaxed);
                        unstarted_model_filter.emplace(std::move(mapping_models));
                        first_reference_mapping = false;
                    } else {
                        std::erase_if(*unstarted_model_filter, [&](size_t model_id) {
                            return mapping_models.count(model_id) == 0;
                        });
                    }
                }
                if (unstarted_model_filter.has_value()) {
                    exact_unstarted_model_candidates_after_prefilter.fetch_add(
                        unstarted_model_filter->size(), std::memory_order_relaxed);
                }
            }
            for (const vg::Mapping& mapping : subpath.path().mapping()) {
                const uint64_t reference_bases =
                    mapping_reference_bases(mapping);
                if (reference_bases == 0) {
                    continue;
                }
                const NodeLedger& node =
                    node_ledger_of(mapping.position().node_id());
                std::vector<State> advanced;
                for (const State& state : states) {
                    if (state.model_id == kUnstarted) {
                        for (const NodeLedger::ExactStep& body_step :
                             node.exact_body_steps) {
                            const auto models = exact_ex50_models_by_body_path.find(
                                body_step.path_handle);
                            if (models == exact_ex50_models_by_body_path.end()) {
                                continue;
                            }
                            for (const size_t model_id : models->second) {
                                if (unstarted_model_filter.has_value() &&
                                    unstarted_model_filter->count(model_id) == 0) {
                                    continue;
                                }
                                const std::optional<State> projected =
                                    project_mapping(state, model_id, node, body_step, mapping,
                                                    reference_bases, true);
                                if (projected.has_value()) {
                                    advanced.push_back(*projected);
                                }
                            }
                        }
                    } else {
                        const ExactEx50Model& model =
                            exact_ex50_models.at(state.model_id);
                        const auto body_steps = exact_step_range(
                            node.exact_body_steps, model.body_path_handle);
                        for (auto step = body_steps.first;
                             step != body_steps.second; ++step) {
                            const std::optional<State> projected =
                                project_mapping(state, state.model_id, node, *step, mapping,
                                                reference_bases, false);
                            if (projected.has_value()) {
                                advanced.push_back(*projected);
                            }
                        }
                    }
                }
                states = std::move(advanced);
                deduplicate_states(states);
                if (states.empty()) {
                    break;
                }
            }

            bool has_outgoing = false;
            auto validate_outgoing = [&](uint32_t next) {
                if (next >= subpath_count || next <= subpath_index) {
                    throw std::runtime_error(
                        "GAMP MultipathAlignment subpaths are not in topological order");
                }
            };
            for (const uint32_t next : subpath.next()) {
                validate_outgoing(next);
                incoming[next].insert(incoming[next].end(), states.begin(), states.end());
                has_outgoing = true;
            }
            for (const vg::Connection& connection : subpath.connection()) {
                const uint32_t next = connection.next();
                validate_outgoing(next);
                for (const State& state : states) {
                    State connected = state;
                    connected.score += connection.score();
                    incoming[next].push_back(std::move(connected));
                }
                has_outgoing = true;
            }
            if (has_outgoing) {
                continue;
            }

            for (const State& state : states) {
                if (state.model_id == kUnstarted ||
                    state.alignment_bases == 0) {
                    continue;
                }
                const ExactEx50Model& model =
                    exact_ex50_models.at(state.model_id);
                const pathtally::Ex50Tier tier = pathtally::classify_ex50_tier(
                    state.exon_overlap, state.alignment_bases,
                    state.splice_concordant);
                const bool direction_forward =
                    state.body_forward ==
                    body_exon_orientation_same(state.model_id);
                const bool body_tier = tier == pathtally::Ex50Tier::Body;
                retain_best_exact_evidence(
                    evidence,
                    {model.target_id,
                     model.exon->annotation.unique_parent,
                     body_tier ? model.body->vg_path_name
                               : model.exon->vg_path_name,
                     body_tier ? model.body->annotation.unique_parent
                               : model.exon->annotation.unique_parent,
                     direction_forward ? 'F' : 'R', tier, state.score});
            }
        }
        return evidence;
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

    std::unique_ptr<BamWriter> bam_writer;
    if (!options.bam_out.empty()) {
        std::string command_line;
        for (int i = 0; i < argc; ++i) {
            // Worker count is an execution detail, not part of the evidence contract.  Omitting it
            // from @PG CL keeps otherwise identical one- and multi-worker BAMs byte-identical.
            if (std::string(argv[i]) == "--threads") {
                ++i;
                continue;
            }
            if (!command_line.empty()) {
                command_line += ' ';
            }
            command_line += argv[i];
        }
        std::string compact_exact_count_strand;
        if (options.compact_exact_count_strand == StrandFilter::Forward) {
            compact_exact_count_strand = "forward";
        } else if (options.compact_exact_count_strand == StrandFilter::Reverse) {
            compact_exact_count_strand = "reverse";
        }
        bam_writer = std::make_unique<BamWriter>(
            options.bam_out, t2g.gene_names, PANCOLLAPSE_VERSION, command_line,
            production_identity && options.count_mode != pathtally::CountMode::Score,
            options.exact_ex50_score_window_enabled, compact_exact_count_strand,
            options.path_identity_ledger_sha256,
            options.strict_allowlisted_parents_sha256,
            options.raw_cb_length, options.raw_umi_length);
    }
    std::unique_ptr<DebugEvidenceWriter> debug_writer;
    if (!options.debug_evidence_out.empty()) {
        debug_writer = std::make_unique<DebugEvidenceWriter>(
            options.debug_evidence_out);
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
    std::atomic<size_t> no_compatible_transcript_groups{0};
    std::atomic<size_t> strand_filtered_groups{0};
    std::atomic<size_t> multigene_dropped_groups{0};
    std::atomic<size_t> unaligned_reads{0};
    std::atomic<size_t> raw_molecule_missing_groups{0};
    std::atomic<size_t> raw_molecule_malformed_groups{0};
    std::atomic<size_t> raw_molecule_unsupported_groups{0};
    std::atomic<size_t> raw_molecule_skipped_groups{0};
    std::atomic<size_t> barcode_only_bam_records{0};
    std::atomic<size_t> strict_allowlisted_evidence_dropped{0};
    // Histogram of emitted-group target-set sizes: target_count -> group_count.
    std::map<size_t, size_t> emitted_target_histogram;
    // completed_names: one entry per read-group name that the producer has closed and submitted.
    // Used exclusively to detect non-contiguous (unsorted) GAMP input: if a closed name recurs,
    // the run aborts rather than silently splitting the read's
    // alignments into separate groups. Peak resident memory is O(number of distinct
    // read-group names seen); on a whole-genome run this can reach many GB. Bounding
    // or removing the set would trade that memory for a silent-correctness risk — a
    // read whose alignments span non-adjacent GAMP positions would be mis-grouped
    // without detection — so the full set is kept as the deliberate memory/correctness
    // tradeoff.  An unordered set preserves exact-name checking without tree-order overhead.
    std::unordered_set<std::string> completed_names;
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

    const auto processing_started = std::chrono::steady_clock::now();
    auto next_progress_report = processing_started + std::chrono::minutes(5);
    size_t next_progress_group = 1000000;
    size_t completed_groups_for_progress = 0;  // touched only while holding the output turn
    OrderedOutputCoordinator ordered_output(options.threads > 1);
    auto process_group = [&](Group group, size_t ordinal,
                             pathtally::TallyMap& tally_workspace) {
        Group& current_group = group;
        const size_t current_read_group = ordinal + 1;
        OrderedOutputCoordinator::Guard output_guard(ordered_output, ordinal);
        auto process = [&]() {
        if (current_group.skip_for_molecule_identity) {
            output_guard.acquire();
            if (debug_writer) {
                debug_writer->write_read(current_read_group, current_group.name, ".", 0);
            }
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
            return;
        }

        bool bam_record_written = false;
        auto write_barcode_only = [&]() {
            if (!bam_writer || bam_record_written) {
                return;
            }
            output_guard.acquire();
            const std::string empty;
            const vg::MultipathAlignment* read =
                current_group.records.empty() ? nullptr : &current_group.records.front();
            bam_writer->write_record(
                current_group.molecule.original_name, /*gene_tid=*/-1,
                read == nullptr || bam_writer->compact_exact() ? empty : read->sequence(),
                read == nullptr || bam_writer->compact_exact() ? empty : read->quality(),
                current_group.molecule.barcode,
                current_group.molecule.umi, current_group.molecule.barcode_quality,
                current_group.molecule.umi_quality,
                /*gx=*/empty, /*gn=*/empty, /*xt=*/empty, /*has_xt=*/false, /*gd=*/empty);
            bam_record_written = true;
            ++barcode_only_bam_records;
        };

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
            if (production_identity) {
                targets = pathtally::select_identity_targets(
                    tally_workspace,
                    [&](const std::string& raw_path) -> pathtally::ResolvedPathIdentity {
                        const auto transcript = t2g.path_transcript.find(raw_path);
                        const auto parent = t2g.path_unique_parent.find(raw_path);
                        if (transcript == t2g.path_transcript.end() ||
                            parent == t2g.path_unique_parent.end()) {
                            throw std::runtime_error("scored graph path " + raw_path +
                                                     " is absent from the path identity ledger");
                        }
                        return {parent->second, transcript->second};
                    });
            } else {
                targets = pathtally::select_targets(
                    tally_workspace, [&](const std::string& raw_path) -> const std::string& {
                        const std::string* transcript =
                            resolve_graph_transcript(t2g, raw_path, false);
                        if (transcript == nullptr) {
                            throw std::runtime_error("scored graph path " + raw_path +
                                                     " is absent from the t2g");
                        }
                        return *transcript;
                    });
            }
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

            // Score-independent aligned-base orientation and touched body ranges are retained for
            // the legacy two-column mode. D063 orientation is derived later from the winning raw
            // paths for the exact transcript/layer that supplied its S/U call, so it skips this
            // mapping traversal entirely.
            std::optional<std::map<uint32_t, std::pair<int64_t, int64_t>>> gene_orient;
            std::optional<std::map<uint32_t, std::pair<int64_t, int64_t>>> body_range;
            if (!body_t2g.transcript_specific) {
                gene_orient.emplace();
                body_range.emplace();
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
                            const NodeLedger::LegacyMetadata& legacy = *nl.legacy;
                            for (const auto& [gene_idx, path_is_reverse] : legacy.gene_orient) {
                                auto& orient = (*gene_orient)[gene_idx];
                                if (read_is_reverse == path_is_reverse) {
                                    orient.first += aligned;
                                } else {
                                    orient.second += aligned;
                                }
                            }
                            for (const uint32_t gene_idx : legacy.body_genes) {
                                auto [it, inserted] = body_range->try_emplace(
                                    gene_idx, std::numeric_limits<int64_t>::max(),
                                    std::numeric_limits<int64_t>::min());
                                it->second.first = std::min(it->second.first, node_id);
                                it->second.second = std::max(it->second.second, node_id);
                            }
                        }
                    }
                }
            }

            // Collapse raw path tallies with MAX, never sum. exon_score is always per canonical
            // transcript. body_score is per gene only for the legacy two-column body t2g and per
            // canonical transcript for D063's three-column body t2g.
            std::map<uint32_t, int64_t> exon_score;
            std::map<uint32_t, int64_t> body_score;
            struct TargetOrientationEvidence {
                int64_t best_score = std::numeric_limits<int64_t>::min();
                int64_t forward_bases = 0;
                int64_t reverse_bases = 0;
            };
            std::map<uint32_t, TargetOrientationEvidence> exon_target_orient;
            std::map<uint32_t, TargetOrientationEvidence> body_target_orient;
            std::map<uint32_t, pathtally::CollapsedIdentityTally> exon_identity_evidence;
            std::map<uint32_t, pathtally::CollapsedIdentityTally> body_identity_evidence;
            auto retain_max_orientation = [](TargetOrientationEvidence& evidence,
                                             const pathtally::HstTally& tally) {
                if (tally.score > evidence.best_score) {
                    evidence.best_score = tally.score;
                    evidence.forward_bases = tally.forward_bases;
                    evidence.reverse_bases = tally.reverse_bases;
                } else if (tally.score == evidence.best_score) {
                    evidence.forward_bases += tally.forward_bases;
                    evidence.reverse_bases += tally.reverse_bases;
                }
            };
            if (production_identity) {
                exon_identity_evidence = pathtally::collapse_identity_tallies(
                    tally_workspace,
                    [&](const std::string& name)
                        -> std::optional<pathtally::NumericPathIdentity> {
                        const auto ex = exon_name_transcript.find(name);
                        if (ex == exon_name_transcript.end()) {
                            return std::nullopt;
                        }
                        return pathtally::NumericPathIdentity{
                            t2g.path_unique_parent.at(name), ex->second};
                    });
                body_identity_evidence = pathtally::collapse_identity_tallies(
                    tally_workspace,
                    [&](const std::string& name)
                        -> std::optional<pathtally::NumericPathIdentity> {
                        const auto body = body_name_transcript.find(name);
                        if (body == body_name_transcript.end()) {
                            return std::nullopt;
                        }
                        const auto& annotation = identity_ledger->rows_by_path.at(name).annotation;
                        if (body_unresolvable_exon_parents.count(
                                annotation.exon_unique_parent) != 0) {
                            return std::nullopt;
                        }
                        return pathtally::NumericPathIdentity{
                            body_t2g.path_unique_parent.at(name), body->second};
                    });
                for (const auto& [target_id, evidence] : exon_identity_evidence) {
                    exon_score.emplace(target_id, evidence.score);
                    exon_target_orient.emplace(
                        target_id,
                        TargetOrientationEvidence{evidence.score, evidence.forward_bases,
                                                  evidence.reverse_bases});
                }
                for (const auto& [target_id, evidence] : body_identity_evidence) {
                    body_score.emplace(target_id, evidence.score);
                    body_target_orient.emplace(
                        target_id,
                        TargetOrientationEvidence{evidence.score, evidence.forward_bases,
                                                  evidence.reverse_bases});
                }
            } else {
                for (const auto& [name, tally] : tally_workspace) {
                    const auto ex = exon_name_transcript.find(name);
                    if (ex != exon_name_transcript.end()) {
                        auto [it, inserted] = exon_score.try_emplace(
                            ex->second, std::numeric_limits<int64_t>::min());
                        it->second = std::max(it->second, tally.score);
                        retain_max_orientation(exon_target_orient[ex->second], tally);
                        continue;
                    }
                    const auto body_transcript = body_name_transcript.find(name);
                    if (body_transcript != body_name_transcript.end()) {
                        auto [it, inserted] = body_score.try_emplace(
                            body_transcript->second, std::numeric_limits<int64_t>::min());
                        it->second = std::max(it->second, tally.score);
                        retain_max_orientation(body_target_orient[body_transcript->second], tally);
                        continue;
                    }
                    const auto body_gene = body_name_gene.find(name);
                    if (body_gene != body_name_gene.end()) {
                        auto [it, inserted] = body_score.try_emplace(
                            body_gene->second, std::numeric_limits<int64_t>::min());
                        it->second = std::max(it->second, tally.score);
                    }
                }
            }

            // Exact body evidence is intentionally unavailable for a degraded Parent.  Do not let
            // its raw body tally turn the preserved exon call into U: that would retain the path
            // but silently change its evidence tier.

            // D061: the read group's own splice-concordant transcript set -- nullopt (unconstrained)
            // unless its alignments actually cross a splice edge (collect_read_node_pairs), in which
            // case only a transcript owning EVERY crossed edge stays eligible for S below.
            const std::vector<std::pair<int64_t, int64_t>> read_node_pairs =
                collect_read_node_pairs(current_group.records);
            const std::optional<std::set<uint32_t>> splice_concordant =
                pathtally::splice_concordant_transcripts(read_node_pairs, splice_edges);

            if (debug_writer) {
                output_guard.acquire();
                std::set<std::pair<int64_t, int64_t>> crossed_splice_edges;
                for (const auto& [a, b] : read_node_pairs) {
                    const auto edge = pathtally::undirected_node_pair(a, b);
                    if (splice_edges.count(edge) != 0) {
                        crossed_splice_edges.insert(edge);
                    }
                }
                debug_writer->write_read(
                    current_read_group, current_group.name,
                    current_group.molecule.original_name, crossed_splice_edges.size());

                int64_t exact_top = std::numeric_limits<int64_t>::min();
                for (const auto& [transcript, score] : exon_score) {
                    exact_top = std::max(exact_top, score);
                }
                for (const auto& [transcript, score] : body_score) {
                    exact_top = std::max(exact_top, score);
                }
                auto is_concordant = [&](uint32_t transcript) {
                    return !splice_concordant.has_value() ||
                           splice_concordant->count(transcript) != 0;
                };
                auto write_exact_top = [&](const char* layer,
                                           const std::map<uint32_t, int64_t>& scores) {
                    for (const auto& [transcript_id, score] : scores) {
                        if (score != exact_top) {
                            continue;
                        }
                        const std::string& transcript =
                            t2g.target_names.at(transcript_id);
                        debug_writer->write_candidate(
                            current_read_group, current_group.name,
                            current_group.molecule.original_name, layer, transcript,
                            t2g.transcript_gene.at(transcript), score,
                            is_concordant(transcript_id));
                    }
                };
                write_exact_top("exon", exon_score);
                write_exact_top("body", body_score);
            }

            ExactEx50EvidenceSet exact_group_evidence(exact_evidence_less);
            if (exact_ex50) {
                for (const vg::MultipathAlignment& record : current_group.records) {
                    ExactEx50EvidenceSet record_evidence =
                        exact_ex50_evidence_for(record);
                    for (const ExactEx50Evidence& evidence : record_evidence) {
                        retain_best_exact_evidence(exact_group_evidence, evidence);
                    }
                }
                // Reference filtering changes the compatible transcript surface, so it must run
                // before the top-score-minus-5 window and before the global E/P/B rank. Filtering
                // a projected winner downstream cannot recover an allowed lower candidate.
                if (!strict_allowlisted_parents.empty()) {
                    const size_t before = exact_group_evidence.size();
                    std::erase_if(
                        exact_group_evidence,
                        [&](const ExactEx50Evidence& evidence) {
                            return strict_allowlisted_parents.count(evidence.parent) == 0;
                        });
                    strict_allowlisted_evidence_dropped.fetch_add(
                        before - exact_group_evidence.size(), std::memory_order_relaxed);
                }
                if (options.exact_ex50_score_window_enabled &&
                    !exact_group_evidence.empty()) {
                    int64_t top_score = std::numeric_limits<int64_t>::min();
                    for (const ExactEx50Evidence& evidence : exact_group_evidence) {
                        top_score = std::max(top_score, evidence.score);
                    }
                    std::erase_if(exact_group_evidence, [&](const ExactEx50Evidence& evidence) {
                        return evidence.score < top_score - kExactEx50ScoreWindow;
                    });
                }
            }

            const std::vector<pathtally::LedgerCall> calls = body_t2g.transcript_specific
                ? pathtally::classify_transcript_body_ledger_group(
                      exon_score, body_score, kIntronFlankBases, splice_concordant)
                : pathtally::classify_ledger_group(exon_score, body_score, *body_range,
                                                   transcript_spans, kIntronFlankBases,
                                                   splice_concordant);

            // D063 keeps orientation per transcript through TX/GD. It uses only the max-scoring
            // raw paths in the layer that supplied the call (exon for S, body for U); tied aliases
            // combine aligned-base evidence with a deterministic forward fallback. Legacy
            // two-column bodies retain D060's per-gene orientation for byte-compatible behavior.
            std::map<uint32_t, char> transcript_gd;  // transcript target id -> 'F'/'R'
            for (const pathtally::LedgerCall& call : calls) {
                const std::string& transcript = t2g.target_names[call.transcript_target_id];
                const std::string& gene = t2g.transcript_gene.at(transcript);
                ledger_genes.insert(gene);
                bool forward = true;
                if (body_t2g.transcript_specific) {
                    const auto& orient_map =
                        call.spliced ? exon_target_orient : body_target_orient;
                    const auto oit = orient_map.find(call.transcript_target_id);
                    forward = oit == orient_map.end() ||
                              oit->second.forward_bases >= oit->second.reverse_bases;
                } else {
                    const uint32_t gene_idx = static_cast<uint32_t>(t2g.gene_ids.at(gene));
                    const auto oit = gene_orient->find(gene_idx);
                    forward = oit == gene_orient->end() || oit->second.first >= oit->second.second;
                }
                transcript_gd[call.transcript_target_id] = forward ? 'F' : 'R';
            }

            // The production normal BAM is a versioned typed union. G rows retain ordinary S/U
            // evidence and carry '.' in exact fields; E rows retain exact Parent evidence and carry
            // '.' in GL. Repeated TX values are evidence rows, not a transcript->gene ambiguity:
            // TX->GX remains single-valued and only identical complete rows deduplicate.
            if (bam_writer) {
                output_guard.acquire();
            }
            if (bam_writer && options.compact_exact_count_strand.has_value() &&
                !exact_group_evidence.empty()) {
                const char wanted =
                    *options.compact_exact_count_strand == StrandFilter::Forward ? 'F' : 'R';
                auto compact_rank = [wanted](const ExactEx50Evidence& evidence) {
                    int base = 2;
                    if (evidence.tier == pathtally::Ex50Tier::FullyExonic) {
                        base = 0;
                    } else if (evidence.tier == pathtally::Ex50Tier::ExonicMajority) {
                        base = 1;
                    }
                    return 2 * base + static_cast<int>(evidence.direction != wanted);
                };
                int winning_rank = std::numeric_limits<int>::max();
                for (const ExactEx50Evidence& evidence : exact_group_evidence) {
                    winning_rank = std::min(winning_rank, compact_rank(evidence));
                }
                std::string xu;
                char winning_gt = '\0';
                char winning_gd = '\0';
                for (const ExactEx50Evidence& evidence : exact_group_evidence) {
                    if (compact_rank(evidence) != winning_rank) {
                        continue;
                    }
                    if (!xu.empty()) {
                        xu += ',';
                    }
                    xu += evidence.parent;
                    winning_gt = pathtally::ex50_tier_code(evidence.tier);
                    winning_gd = evidence.direction;
                }
                bam_writer->write_compact_exact_record(
                    current_group.molecule.original_name, current_group.molecule.barcode,
                    current_group.molecule.umi, current_group.molecule.barcode_quality,
                    current_group.molecule.umi_quality, winning_gt, winning_gd, xu);
                bam_record_written = true;
            } else if (bam_writer && !options.compact_exact_count_strand.has_value() &&
                production_identity &&
                (!calls.empty() || !exact_group_evidence.empty())) {
                std::string xr, tx, gx, gd, gl, gt, xp, xu, primary_gene;
                int32_t primary_tid = -1;
                std::set<std::string> bam_genes;
                size_t n = 0;
                for (const pathtally::LedgerCall& call : calls) {
                    const std::string& transcript = t2g.target_names[call.transcript_target_id];
                    const std::string& gene = t2g.transcript_gene.at(transcript);
                    if (n++ > 0) {
                        xr += ';'; tx += ';'; gx += ';'; gd += ';'; gl += ';'; gt += ';'; xp += ';'; xu += ';';
                    }
                    xr += 'G';
                    tx += transcript;
                    gx += gene;
                    gd += transcript_gd.at(call.transcript_target_id);
                    gl += call.spliced ? 'S' : 'U';
                    gt += '.';
                    const auto& evidence_map =
                        call.spliced ? exon_identity_evidence : body_identity_evidence;
                    const auto evidence = evidence_map.find(call.transcript_target_id);
                    if (evidence == evidence_map.end()) {
                        throw std::runtime_error(
                            "internal path identity provenance is missing for a ledger call");
                    }
                    xp += join_sorted(evidence->second.winning_paths, ',');
                    xu += join_sorted(evidence->second.winning_parents, ',');
                    bam_genes.insert(gene);
                    if (primary_gene.empty()) {
                        primary_gene = gene;
                        primary_tid = t2g.gene_ids.at(gene);
                    }
                }
                for (const ExactEx50Evidence& evidence : exact_group_evidence) {
                    const std::string& transcript =
                        t2g.target_names[evidence.target_id];
                    const std::string& gene =
                        t2g.transcript_gene.at(transcript);
                    if (n++ > 0) {
                        xr += ';'; tx += ';'; gx += ';'; gd += ';'; gl += ';'; gt += ';'; xp += ';'; xu += ';';
                    }
                    xr += 'X';
                    tx += transcript;
                    gx += gene;
                    gd += evidence.direction;
                    gl += '.';
                    gt += pathtally::ex50_tier_code(evidence.tier);
                    xp += evidence.path;
                    xu += evidence.parent;
                    bam_genes.insert(gene);
                    if (primary_gene.empty()) {
                        primary_gene = gene;
                        primary_tid = t2g.gene_ids.at(gene);
                    }
                }
                const bool multigene = bam_genes.size() > 1;
                const bool has_xt = !multigene || options.bam_multigene == BamMultiGenePolicy::First;
                const vg::MultipathAlignment& read =
                    current_group.records.front();
                if (!multigene || options.bam_multigene != BamMultiGenePolicy::Omit) {
                    bam_writer->write_record(
                        current_group.molecule.original_name, primary_tid,
                        read.sequence(), read.quality(),
                        current_group.molecule.barcode, current_group.molecule.umi,
                        current_group.molecule.barcode_quality,
                        current_group.molecule.umi_quality, gx,
                        /*gn=*/gx, /*xt=*/primary_gene, /*has_xt=*/has_xt,
                        /*gd=*/gd, /*gl=*/gl, /*gt=*/gt, /*tx=*/tx, /*xp=*/xp,
                        /*xu=*/xu, /*xr=*/xr);
                    bam_record_written = true;
                }
            } else if (bam_writer && !exact_ex50 && !calls.empty()) {
                std::string tx, gx, gd, gl, xp, xu, primary_gene;
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
                        if (production_identity) {
                            xp += ';';
                            xu += ';';
                        }
                    }
                    tx += transcript;
                    gx += gene;
                    gd += transcript_gd.at(call.transcript_target_id);
                    gl += call.spliced ? 'S' : 'U';
                    if (production_identity) {
                        const auto& evidence_map =
                            call.spliced ? exon_identity_evidence : body_identity_evidence;
                        const auto evidence = evidence_map.find(call.transcript_target_id);
                        if (evidence == evidence_map.end()) {
                            throw std::runtime_error(
                                "internal path identity provenance is missing for a ledger call");
                        }
                        xp += join_sorted(evidence->second.winning_paths, ',');
                        xu += join_sorted(evidence->second.winning_parents, ',');
                    }
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
                                             current_group.molecule.barcode_quality,
                                             current_group.molecule.umi_quality, gx,
                                             /*gn=*/gx, /*xt=*/primary_gene, /*has_xt=*/has_xt,
                                             /*gd=*/gd, /*gl=*/gl, /*gt=*/"", /*tx=*/tx, /*xp=*/xp,
                                             /*xu=*/xu);
                    bam_record_written = true;
                }
            }

            // RAD/alevin-fry: the emitted transcripts are the read's equivalence class.
            for (const pathtally::LedgerCall& call : calls) {
                targets.push_back({t2g.target_names[call.transcript_target_id],
                                   transcript_gd.at(call.transcript_target_id) == 'F', {}, {}});
            }
        }

        if (targets.empty()) {
            ++no_compatible_transcript_groups;
            if (current_group.saw_unaligned_record && !current_group.saw_subpath_record) {
                ++unaligned_reads;
            }
            write_barcode_only();
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
                write_barcode_only();
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
            write_barcode_only();
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
            output_guard.acquire();
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
            std::string tx, xp, xu;
            if (production_identity) {
                for (const pathtally::RadTarget& target : targets) {
                    if (!tx.empty()) {
                        tx += ';';
                        xp += ';';
                        xu += ';';
                    }
                    tx += target.transcript;
                    std::set<std::string> paths(target.winning_paths.begin(),
                                                target.winning_paths.end());
                    std::set<std::string> parents(target.winning_parents.begin(),
                                                  target.winning_parents.end());
                    xp += join_sorted(paths, ',');
                    xu += join_sorted(parents, ',');
                }
            }
            const vg::MultipathAlignment& read = current_group.records.front();
            bam_writer->write_record(current_group.molecule.original_name,
                                     t2g.gene_ids.at(primary_gene), read.sequence(), read.quality(),
                                     current_group.molecule.barcode, current_group.molecule.umi,
                                     current_group.molecule.barcode_quality,
                                     current_group.molecule.umi_quality, gx,
                                     /*gn=*/gx, /*xt=*/primary_gene, has_xt, /*gd=*/gd,
                                     /*gl=*/"", /*gt=*/"", /*tx=*/tx, /*xp=*/xp, /*xu=*/xu);
            bam_record_written = true;
        }
        // hits stays empty for a ledger multi-gene read (RAD skipped even when the BAM rescues
        // it), so the RAD emitted-target histogram reflects only actual RAD records.
        if (!ledger_multigene) {
            ++emitted_target_histogram[hits.size()];
        }
        write_barcode_only();
        };
        process();
        // Even groups with no output must take and retire their ordinal so later writers progress.
        output_guard.acquire();
        ++completed_groups_for_progress;
        const bool group_report = completed_groups_for_progress >= next_progress_group;
        if (group_report || completed_groups_for_progress % 4096 == 0) {
            const auto progress_now = std::chrono::steady_clock::now();
            if (group_report || progress_now >= next_progress_report) {
                const double elapsed = std::chrono::duration<double>(
                    progress_now - processing_started).count();
                std::cerr << "panCollapse: progress: completed_read_groups="
                          << completed_groups_for_progress << " elapsed_seconds=" << elapsed
                          << " groups_per_second="
                          << (elapsed > 0.0 ? completed_groups_for_progress / elapsed : 0.0)
                          << '\n';
                next_progress_report = progress_now + std::chrono::minutes(5);
                next_progress_group = completed_groups_for_progress + 1000000;
            }
        }
    };

    // Keep at most two queued groups per worker.  Active groups add at most one more per worker,
    // so scheduling memory is O(threads) and independent of the total GAMP length.
    const size_t max_queued_groups =
        options.threads > std::numeric_limits<size_t>::max() / 2
            ? std::numeric_limits<size_t>::max()
            : std::max<size_t>(1, options.threads * 2);
    std::mutex jobs_mutex;
    std::condition_variable jobs_ready;
    std::condition_variable jobs_have_space;
    std::deque<GroupJob> jobs;
    bool jobs_closed = false;
    bool stop_workers = false;
    std::vector<std::thread> workers;
    pathtally::TallyMap serial_tally_workspace;

    auto worker_loop = [&]() {
        pathtally::TallyMap tally_workspace;
        try {
            while (true) {
                GroupJob job;
                {
                    std::unique_lock<std::mutex> lock(jobs_mutex);
                    jobs_ready.wait(lock, [&] {
                        return stop_workers || !jobs.empty() || jobs_closed;
                    });
                    if (stop_workers || (jobs.empty() && jobs_closed)) {
                        return;
                    }
                    job = std::move(jobs.front());
                    jobs.pop_front();
                }
                jobs_have_space.notify_one();
                process_group(std::move(job.group), job.ordinal, tally_workspace);
            }
        } catch (...) {
            ordered_output.fail(std::current_exception());
            {
                std::lock_guard<std::mutex> lock(jobs_mutex);
                stop_workers = true;
            }
            jobs_ready.notify_all();
            jobs_have_space.notify_all();
        }
    };

    if (options.threads > 1) {
        workers.reserve(options.threads);
        try {
            for (size_t i = 0; i < options.threads; ++i) {
                workers.emplace_back(worker_loop);
            }
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(jobs_mutex);
                stop_workers = true;
                jobs_closed = true;
            }
            jobs_ready.notify_all();
            for (std::thread& worker : workers) {
                worker.join();
            }
            throw;
        }
    }

    auto submit_group = [&]() {
        if (!have_group) {
            return;
        }
        const size_t ordinal = input_read_groups - 1;
        completed_names.insert(current_group.name);
        GroupJob job{ordinal, std::move(current_group)};
        have_group = false;
        if (options.threads == 1) {
            process_group(std::move(job.group), job.ordinal, serial_tally_workspace);
            return;
        }

        std::unique_lock<std::mutex> lock(jobs_mutex);
        jobs_have_space.wait(lock, [&] {
            return stop_workers || jobs.size() < max_queued_groups;
        });
        if (stop_workers) {
            lock.unlock();
            const std::exception_ptr failure = ordered_output.failure();
            if (failure) {
                std::rethrow_exception(failure);
            }
            throw std::runtime_error("PanCollapse worker pool stopped without an exception");
        }
        jobs.push_back(std::move(job));
        lock.unlock();
        jobs_ready.notify_one();
    };

    std::exception_ptr producer_failure;
    try {
        vg::io::for_each<vg::MultipathAlignment>(*gamp_in, [&](vg::MultipathAlignment& alignment) {
            ++input_records;
            if (!have_group) {
                start_group(alignment);
            } else if (alignment.name() != current_group.name) {
                submit_group();
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
        submit_group();
    } catch (...) {
        producer_failure = std::current_exception();
        ordered_output.fail(producer_failure);
        std::lock_guard<std::mutex> lock(jobs_mutex);
        stop_workers = true;
    }

    if (options.threads > 1) {
        {
            std::lock_guard<std::mutex> lock(jobs_mutex);
            jobs_closed = true;
        }
        jobs_ready.notify_all();
        jobs_have_space.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }
    if (producer_failure) {
        std::rethrow_exception(producer_failure);
    }
    if (const std::exception_ptr worker_failure = ordered_output.failure()) {
        std::rethrow_exception(worker_failure);
    }

    const auto processing_finished = std::chrono::steady_clock::now();

    if (input_records == 0) {
        throw std::runtime_error("panCollapse convert expects at least one GAMP record");
    }

    if (exact_ex50) {
        std::cerr << "panCollapse: exact Ex50 unstarted model candidates: before_prefilter="
                  << exact_unstarted_model_candidates_before_prefilter.load(std::memory_order_relaxed)
                  << " after_prefilter="
                  << exact_unstarted_model_candidates_after_prefilter.load(std::memory_order_relaxed)
                  << '\n';
    }
    const double initialization_seconds = std::chrono::duration<double>(
        processing_started - invocation_started).count();
    const double processing_seconds = std::chrono::duration<double>(
        processing_finished - processing_started).count();
    std::cerr << "panCollapse: performance: workers=" << options.threads
              << " initialization_seconds=" << initialization_seconds
              << " processing_seconds=" << processing_seconds
              << " input_records=" << input_records
              << " input_read_groups=" << input_read_groups
              << " groups_per_second="
              << (processing_seconds > 0.0 ? input_read_groups / processing_seconds : 0.0)
              << '\n';

    rad_writer.finalize();
    if (bam_writer) {
        bam_writer->finalize();
    }
    if (debug_writer) {
        debug_writer->finalize();
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
    const std::string exact_ex50_score_window =
        exact_ex50
            ? (options.exact_ex50_score_window_enabled
                   ? std::to_string(kExactEx50ScoreWindow)
                   : std::string("disabled"))
            : std::string("not_applicable");
    write_text_file(options.out_dir / "summary.tsv",
                    "input_records\t" + std::to_string(input_records) + "\ninput_read_groups\t" +
                        std::to_string(input_read_groups) + "\nemitted_groups\t" +
                        std::to_string(rad_writer.record_count()) + "\nno_compatible_transcript_groups\t" +
                        std::to_string(no_compatible_transcript_groups.load()) + "\nstrand_filtered_groups\t" +
                        std::to_string(strand_filtered_groups.load()) + "\nmultigene_dropped_groups\t" +
                        std::to_string(multigene_dropped_groups.load()) + "\nunaligned_reads\t" +
                        std::to_string(unaligned_reads.load()) + "\nraw_molecule_missing_groups\t" +
                        std::to_string(raw_molecule_missing_groups.load()) + "\nraw_molecule_malformed_groups\t" +
                        std::to_string(raw_molecule_malformed_groups.load()) + "\nraw_molecule_unsupported_groups\t" +
                        std::to_string(raw_molecule_unsupported_groups.load()) + "\nraw_molecule_skipped_groups\t" +
                        std::to_string(raw_molecule_skipped_groups.load()) + "\nbam_records\t" +
                        std::to_string(bam_writer ? bam_writer->record_count() : 0) +
                        "\nbarcode_only_bam_records\t" +
                        std::to_string(barcode_only_bam_records.load()) + "\ngrouping_recurrence_failures\t" +
                        std::to_string(grouping_recurrence_failures) +
                        "\nexact_ex50_score_window\t" + exact_ex50_score_window +
                        "\ncompatible_parent_policy\t" +
                        (strict_allowlisted_parents.empty()
                             ? std::string("unfiltered")
                             : std::string("strict-allowlisted-v1")) +
                        "\ncompatible_parent_allowlist_size\t" +
                        std::to_string(strict_allowlisted_parents.size()) +
                        "\nstrict_allowlisted_evidence_dropped\t" +
                        std::to_string(strict_allowlisted_evidence_dropped.load()) +
                        "\nbody_paths_degraded\t" +
                        std::to_string(body_unresolvable_paths.size()) +
                        "\nbody_path_degrade_reason\t" +
                        (body_unresolvable_paths.empty()
                             ? std::string("none")
                             : std::string("repeated_exonic_node")) +
                        "\nemitted_target_count_histogram\t" + histogram_value + "\n");
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
