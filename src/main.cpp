#include <handlegraph/path_position_handle_graph.hpp>
#include <handlegraph/util.hpp>
#include <htslib/sam.h>
#include <simdjson.h>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include "gamp_alignment_complexity.hpp"
#include "graph_input.hpp"
#include "native_count_index.hpp"
#include "direct_count_index.hpp"
#include "direct_count_compile.hpp"
#include "prepared_count.hpp"
#include "count_memory_budget.hpp"
#include "bounded_reference_cache.hpp"

#include "pathtally.hpp"
#include "node_tally_cache.hpp"
#include "pathtally_ledger.hpp"
#include "pathtally_qualadj.hpp"
#include "path_identity_ledger.hpp"
#include "direct_count.hpp"
#include "direct_count_facts.hpp"
#include "annotation_fact_index.hpp"
#include "score_dedup.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <omp.h>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
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
#include <string_view>
#include <thread>
#ifdef __linux__
#include <pthread.h>
#endif
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
    bool count_preload_only = false;
    std::filesystem::path gamp;
    std::filesystem::path xg;
    std::filesystem::path gbz;
    std::filesystem::path gbwt;
    std::filesystem::path r_index;
    std::filesystem::path r_index_manifest;
    pancollapse::count_memory::Request memory;
    bool trust_r_index_manifest = false;
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
    std::filesystem::path annotation_index;
    std::filesystem::path barcode_whitelist;
    std::vector<std::string> requested_profiles;
    std::vector<pancollapse::direct_count::ProfileOverride> profile_overrides;
    std::vector<pancollapse::direct_count::EffectiveProfile> count_profiles;
    std::string analysis_scope;
    bool count_10x_mex = false;
    std::uint64_t max_alignments_per_read =
        pancollapse::input::kDefaultMaxAlignmentsPerRead;
};

struct RIndexBinding {
    std::string gbwt_sha256;
    std::uint64_t gbwt_size_bytes = 0;
    std::string r_index_sha256;
    std::uint64_t r_index_size_bytes = 0;
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
    "Supported workflow: panCollapse index -> panCollapse count\n"
    "Deprecated BAM conversion: panCollapse convert\n\n"
    "usage: panCollapse convert --gamp reads.gamp|- (--xg graph.xg [--gbwt paths.gbwt] | --gbz graph.gbz) "
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
    "[--strict-allowlisted-parents-sha256 HEX]\n"
    "       panCollapse count --gamp reads.gamp --annotation-index direct.pci "
    "--barcode-whitelist barcodes.txt PROFILE_SELECTOR --out-dir counts "
    "[--profile-override BASE:FIELD=VALUE --analysis-scope sensitivity-analysis] "
    "[--threads N] [--raw-cb-length N] [--raw-umi-length N] "
    "[--max-alignments-per-read N] [--molecule-identity-failures skip|fail] "
    "[--preload-only] [--10x-mex] [--memory-budget SIZE] [--cache-limit assignment=SIZE]\n"
    "       panCollapse index --annotation corrected.gff3.gz --annotation-reference reference.gff3.gz "
    "--path-identity-ledger ledger.tsv --out facts.pci "
    "[--annotation-format gff3|gtf] [--annotation-reference-format gff3|gtf] [--threads N]\n"
    "       panCollapse index --from-annotation-index old-v2.pci --path-identity-ledger ledger.tsv "
    "--out reference.pci [--threads N]\n"
    "       panCollapse index (--from-annotation-index annotation-v3.pci | --count-bundle bundle) "
    "--gbwt paths.gbwt --out native.pci [--threads N]\n"
    "       panCollapse index --from-annotation-index native.pci --xg graph.xg "
    "--gbwt paths.gbwt --r-index paths.ri --r-index-manifest binding.json "
    "--out direct.pci [--threads N] [--memory-budget SIZE]\n"
    "       Direct facts contain all reference lookups; count needs no XG/GBWT/RI.\n"
    "       PROFILE_SELECTOR is --cr7, --pansc-strict-v1, or --profile ID; "
    "selectors may be combined";

[[noreturn]] void usage_error() {
    throw std::runtime_error(kUsageText);
}

bool is_lower_sha256(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

std::string reference_json_string(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (const unsigned char ch : value) {
        if (ch == '"' || ch == '\\') { result += '\\'; result += static_cast<char>(ch); }
        else if (ch < 32) { result += "\\u00"; result += hex[ch >> 4]; result += hex[ch & 15]; }
        else result += static_cast<char>(ch);
    }
    return result + '"';
}

void require_r_index_keys(simdjson::dom::object object) {
    const std::array<std::string_view, 5> expected{
        "schema", "gbwt_sha256", "gbwt_size_bytes", "r_index_sha256", "r_index_size_bytes"};
    std::vector<std::string> keys;
    for (const auto field : object) keys.emplace_back(field.key);
    std::sort(keys.begin(), keys.end());
    std::vector<std::string> wanted;
    for (const auto key : expected) wanted.emplace_back(key);
    std::sort(wanted.begin(), wanted.end());
    if (keys != wanted) {
        throw std::runtime_error("r-index manifest has missing, duplicate, or unexpected fields");
    }
}

RIndexBinding read_r_index_manifest(const std::filesystem::path& path) {
    try {
        simdjson::dom::parser parser;
        const simdjson::dom::element document = parser.load(path.string());
        const auto object = document.get_object().value();
        require_r_index_keys(object);
        const auto schema = std::string(static_cast<std::string_view>(document["schema"]));
        if (schema != "pancollapse-r-index-binding-v1") {
            throw std::runtime_error("r-index manifest has unsupported schema");
        }
        RIndexBinding result{
            std::string(static_cast<std::string_view>(document["gbwt_sha256"])),
            static_cast<std::uint64_t>(document["gbwt_size_bytes"]),
            std::string(static_cast<std::string_view>(document["r_index_sha256"])),
            static_cast<std::uint64_t>(document["r_index_size_bytes"])};
        if (!is_lower_sha256(result.gbwt_sha256) || !is_lower_sha256(result.r_index_sha256) ||
            result.gbwt_size_bytes == 0 || result.r_index_size_bytes == 0) {
            throw std::runtime_error("r-index manifest has invalid SHA-256 or byte size");
        }
        return result;
    } catch (const simdjson::simdjson_error& error) {
        throw std::runtime_error("invalid r-index manifest: " + std::string(error.what()));
    }
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

std::uint64_t parse_max_alignments_per_read(const std::string& value) {
    constexpr std::string_view option_name = "--max-alignments-per-read";
    if (value.empty()) {
        throw std::runtime_error(std::string(option_name) + " must be an unsigned integer");
    }
    std::uint64_t parsed = 0;
    for (const char c : value) {
        if (c < '0' || c > '9') {
            throw std::runtime_error(std::string(option_name) + " must be an unsigned integer");
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            throw std::runtime_error(std::string(option_name) + " is outside the supported integer range");
        }
        parsed = parsed * 10 + digit;
    }
    if (parsed == std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error(std::string(option_name) + " must be below UINT64_MAX");
    }
    return parsed;
}

pancollapse::direct_count::AnnotationFormat parse_annotation_format(const std::string& value) {
    using pancollapse::direct_count::AnnotationFormat;
    if (value == "gff3") return AnnotationFormat::gff3;
    if (value == "gtf") return AnnotationFormat::gtf;
    throw std::runtime_error("annotation format must be gff3 or gtf");
}

Options parse_native_count_options(int argc, char** argv) {
    Options options;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const char* name) {
            if (arg != name || i + 1 >= argc) usage_error();
            return std::string(argv[++i]);
        };
        if (arg == "--gamp") options.gamp = require_value("--gamp");
        else if (arg == "--xg" || arg == "--gbwt" || arg == "--r-index" ||
                 arg == "--r-index-manifest" || arg == "--trust-r-index-manifest")
            throw std::runtime_error(arg + " is an index-time input, not a direct count option; rebuild using panCollapse index");
        else if (arg == "--annotation-index") options.annotation_index = require_value("--annotation-index");
        else if (arg == "--barcode-whitelist") options.barcode_whitelist = require_value("--barcode-whitelist");
        else if (arg == "--out-dir") options.out_dir = require_value("--out-dir");
        else if (arg == "--threads") options.threads = parse_size_option("--threads", require_value("--threads"));
        else if (arg == "--raw-cb-length") options.raw_cb_length = parse_size_option("--raw-cb-length", require_value("--raw-cb-length"));
        else if (arg == "--raw-umi-length") options.raw_umi_length = parse_size_option("--raw-umi-length", require_value("--raw-umi-length"));
        else if (arg == "--max-alignments-per-read") options.max_alignments_per_read =
            parse_max_alignments_per_read(require_value("--max-alignments-per-read"));
        else if (arg == "--molecule-identity-failures") {
            const auto value = require_value("--molecule-identity-failures");
            if (value == "skip") options.molecule_identity_failures = MoleculeIdentityFailurePolicy::Skip;
            else if (value == "fail") options.molecule_identity_failures = MoleculeIdentityFailurePolicy::Fail;
            else throw std::runtime_error("--molecule-identity-failures must be skip or fail");
        } else if (arg == "--cr7") options.requested_profiles.emplace_back("cr7-v1");
        else if (arg == "--pansc-strict-v1") options.requested_profiles.emplace_back("pansc-strict-v1");
        else if (arg == "--profile") options.requested_profiles.push_back(require_value("--profile"));
        else if (arg == "--profile-override") options.profile_overrides.push_back(
            pancollapse::direct_count::parse_profile_override(require_value("--profile-override")));
        else if (arg == "--analysis-scope") options.analysis_scope = require_value("--analysis-scope");
        else if (arg == "--preload-only") options.count_preload_only = true;
        else if (arg == "--10x-mex") options.count_10x_mex = true;
        else if (arg == "--memory-budget") {
            if (options.memory.target_bytes) throw std::runtime_error("duplicate --memory-budget");
            const auto value = require_value("--memory-budget");
            try { options.memory.target_bytes = pancollapse::count_memory::parse_bytes(value); }
            catch (const std::runtime_error& error) {
                throw std::runtime_error(std::string("--memory-budget: ") + error.what());
            }
        } else if (arg == "--cache-limit") {
            const auto value = require_value("--cache-limit");
            const auto split = value.find('=');
            if (split == std::string::npos) throw std::runtime_error("--cache-limit requires KIND=SIZE");
            try {
                pancollapse::count_memory::set_cache_override(
                    options.memory, value.substr(0, split), value.substr(split + 1));
            } catch (const std::runtime_error& error) {
                throw std::runtime_error(std::string("--cache-limit: ") + error.what());
            }
        }
        else if (arg == "--assignment-scoring")
            throw std::runtime_error("--assignment-scoring was removed: native count always uses node-score-v1");
        else throw std::runtime_error("unsupported count option: " + arg);
    }
    if (options.gamp.empty() || options.annotation_index.empty() ||
        options.barcode_whitelist.empty() || options.out_dir.empty()) {
        throw std::runtime_error("direct count requires --gamp, --annotation-index, --barcode-whitelist, and --out-dir");
    }
    if (!pancollapse::direct_index::is_index(options.annotation_index)) {
        throw std::runtime_error("direct count requires a complete direct-facts --annotation-index; rebuild using panCollapse index with --xg/--gbwt/--r-index");
    }
    for (const auto kind : {pancollapse::count_memory::CacheKind::node,
                            pancollapse::count_memory::CacheKind::edge,
                            pancollapse::count_memory::CacheKind::splice})
        if (options.memory.overrides[static_cast<std::size_t>(kind)])
            throw std::runtime_error("direct count only supports --cache-limit assignment=SIZE");
    if (options.threads == 0 || options.raw_cb_length == 0 || options.raw_cb_length > 32 ||
        options.raw_umi_length == 0 || options.raw_umi_length > 32) {
        throw std::runtime_error("native count threads must be positive and raw barcode/UMI lengths must be in 1..32");
    }
    if (options.requested_profiles.empty()) {
        throw std::runtime_error("native count requires at least one profile");
    }
    for (std::string& requested : options.requested_profiles) {
        requested = pancollapse::direct_count::profile(
            pancollapse::direct_count::parse_profile_id(requested)).id_string;
    }
    std::sort(options.requested_profiles.begin(), options.requested_profiles.end());
    if (std::adjacent_find(options.requested_profiles.begin(), options.requested_profiles.end()) !=
        options.requested_profiles.end()) {
        throw std::runtime_error("native count profile IDs must not be repeated");
    }
    if (!options.profile_overrides.empty() && options.analysis_scope != "sensitivity-analysis") {
        throw std::runtime_error("--profile-override requires --analysis-scope sensitivity-analysis");
    }
    if (!options.analysis_scope.empty() && options.analysis_scope != "sensitivity-analysis") {
        throw std::runtime_error("the only native-count analysis scope is sensitivity-analysis");
    }
    for (const std::string& requested : options.requested_profiles) {
        const auto id = pancollapse::direct_count::parse_profile_id(requested);
        std::vector<pancollapse::direct_count::ProfileOverride> overrides;
        std::copy_if(options.profile_overrides.begin(), options.profile_overrides.end(),
                     std::back_inserter(overrides), [&](const auto& override) {
                         return override.base == id;
                     });
        options.count_profiles.push_back(pancollapse::direct_count::effective_profile(id, overrides));
    }
    for (const auto& override : options.profile_overrides) {
        if (std::none_of(options.count_profiles.begin(), options.count_profiles.end(),
                         [&](const auto& profile) { return profile.profile.id == override.base; })) {
            throw std::runtime_error("profile override base was not selected by --profile or its alias");
        }
    }
    options.out_dir = std::filesystem::absolute(options.out_dir).lexically_normal();
    return options;
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
        } else if (arg == "--gbz") {
            options.gbz = require_value("--gbz");
        } else if (arg == "--gbwt") {
            options.gbwt = require_value("--gbwt");
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

    if (options.out_dir.empty()) {
        usage_error();
    }
    if (options.gamp.empty()) usage_error();
    if ((!options.gbz.empty() && (!options.xg.empty() || !options.gbwt.empty())) ||
        (options.gbz.empty() && options.xg.empty())) {
        throw std::runtime_error(
            "requires exactly one graph source: --xg FILE [--gbwt FILE] or --gbz FILE; "
            "--gbwt requires --xg");
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

MoleculeParseResult parse_molecule_id(const std::string& name, size_t cb_length,
                                      size_t umi_length) {
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
            molecule_name.resize(quality_sep);
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
                molecule_name.resize(quality_sep);
                throw std::invalid_argument("raw " + label +
                                            " quality contains non-hexadecimal text");
            }
            const char quality = static_cast<char>((hi << 4) | lo);
            const unsigned char printable = static_cast<unsigned char>(quality);
            if (printable < 33 || printable > 126) {
                molecule_name.resize(quality_sep);
                throw std::invalid_argument("decoded raw " + label +
                                            " quality is not printable FASTQ quality text");
            }
            decoded.push_back(quality);
        }
        molecule_name.resize(quality_sep);
        return true;
    };
    // Failure results still carry the read-name prefix when the name has the
    // <prefix>_<CB>_<UMI> shape, so diagnostics can report one QNAME grammar.
    auto failure = [&](MoleculeParseStatus status, std::string message) {
        MoleculeId partial;
        // A failure in the first (UMI) quality pass leaves a well-formed barcode
        // quality field attached; drop any trailing quality fields first.
        std::string_view trimmed = molecule_name;
        for (int pass = 0; pass < 2; ++pass) {
            const size_t sep = trimmed.rfind('_');
            if (sep == std::string_view::npos) {
                break;
            }
            const std::string_view field = trimmed.substr(sep + 1);
            if (!field.starts_with("cy") && !field.starts_with("uy")) {
                break;
            }
            trimmed = trimmed.substr(0, sep);
        }
        const size_t last_sep = trimmed.rfind('_');
        if (last_sep != std::string_view::npos && last_sep > 0) {
            const size_t prior_sep = trimmed.rfind('_', last_sep - 1);
            if (prior_sep != std::string_view::npos) {
                partial.original_name = std::string(trimmed.substr(0, prior_sep));
            }
        }
        return MoleculeParseResult{status, std::move(partial), std::move(message)};
    };
    try {
        const bool has_uy = decode_quality_suffix("uy", umi_length, umi_quality, "UMI");
        const bool has_cy = decode_quality_suffix("cy", cb_length, barcode_quality, "barcode");
        if (has_uy && !has_cy) {
            return failure(MoleculeParseStatus::Malformed,
                           "raw UMI quality suffix requires the preceding barcode quality suffix");
        }
    } catch (const std::invalid_argument& error) {
        return failure(MoleculeParseStatus::Malformed, error.what());
    }

    const size_t umi_sep = molecule_name.rfind('_');
    if (umi_sep == std::string::npos) {
        return {MoleculeParseStatus::Missing, {}, "GAMP name does not contain raw UMI"};
    }
    if (umi_sep == 0 || umi_sep + 1 == molecule_name.size()) {
        return failure(MoleculeParseStatus::Missing, "GAMP name has an empty raw CB or UMI field");
    }
    const size_t cb_sep = molecule_name.rfind('_', umi_sep - 1);
    if (cb_sep == std::string::npos) {
        return failure(MoleculeParseStatus::Missing, "GAMP name does not contain raw CB");
    }

    MoleculeId id{molecule_name.substr(0, cb_sep),
                  molecule_name.substr(cb_sep + 1, umi_sep - cb_sep - 1),
                  molecule_name.substr(umi_sep + 1), barcode_quality, umi_quality};
    if (id.barcode.empty() || id.umi.empty()) {
        return {MoleculeParseStatus::Missing, MoleculeId{id.original_name, {}, {}, {}, {}}, "GAMP name has an empty raw CB or UMI field"};
    }
    if (id.original_name.empty()) {
        return {MoleculeParseStatus::Malformed, MoleculeId{id.original_name, {}, {}, {}, {}}, "GAMP name has no original read-name prefix"};
    }
    if (id.barcode.size() != cb_length || id.umi.size() != umi_length) {
        return {MoleculeParseStatus::Malformed, MoleculeId{id.original_name, {}, {}, {}, {}}, "raw CB/UMI lengths do not match configured lengths"};
    }
    for (const char base : id.barcode) {
        if (!is_supported_molecule_base(base)) {
            return {MoleculeParseStatus::Unsupported, MoleculeId{id.original_name, {}, {}, {}, {}}, "raw CB contains unsupported base"};
        }
    }
    for (const char base : id.umi) {
        if (!is_supported_molecule_base(base)) {
            return {MoleculeParseStatus::Unsupported, MoleculeId{id.original_name, {}, {}, {}, {}}, "raw UMI contains unsupported base"};
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

std::string join_sorted(std::vector<const std::string*> values, char delimiter) {
    std::sort(values.begin(), values.end(),
              [](const std::string* a, const std::string* b) { return *a < *b; });
    values.erase(std::unique(values.begin(), values.end(),
                             [](const std::string* a, const std::string* b) {
                                 return *a == *b;
                             }),
                 values.end());
    std::string joined;
    for (const std::string* value : values) {
        if (!joined.empty()) {
            joined += delimiter;
        }
        joined += *value;
    }
    return joined;
}

std::string join_sorted(const std::vector<pathtally::RankedParent>& parents,
                        char delimiter) {
    std::vector<const std::string*> names;
    names.reserve(parents.size());
    for (const auto& parent : parents) names.push_back(parent.unique_parent);
    return join_sorted(std::move(names), delimiter);
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
    const std::vector<const vg::MultipathAlignment*>& records) {
    std::vector<std::pair<int64_t, int64_t>> pairs;
    for (const vg::MultipathAlignment* record_pointer : records) {
        const vg::MultipathAlignment& record = *record_pointer;
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
    // The read's aligned records point into `owned_records`.
    std::vector<const vg::MultipathAlignment*> records;
    std::vector<vg::MultipathAlignment> owned_records;
    bool saw_subpath_record = false;
    bool saw_unaligned_record = false;
};

struct GroupJob {
    size_t ordinal = 0;
    Group group;
    std::chrono::steady_clock::time_point submitted_at;
};

// Workers perform read-local computation independently, while every externally visible write is
// serialized by input ordinal. A guard keeps the turn from its first write through the end of the
// group, so RAD chunks, BAM records, debug rows, warnings, and histograms retain the one-thread
// order. Read-local exact DP finishes before debug output acquires the turn, avoiding accidental
// compute serialization while preserving audit order. An exception aborts all waiters instead of
// letting a later ordinal deadlock forever.
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
            // During unwinding the worker catch records the original exception and then notifies.
            // Delaying this wake prevents another waiter from racing in with a generic abort error.
            if (!unwinding) {
                coordinator_.cv_.notify_all();
            }
            lock_.unlock();
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

// Run contiguous index blocks in parallel, but retain failures by block ordinal and rethrow the
// first one only after every worker has joined. A ring retains at most two result payloads per
// worker and commits them in ordinal order, so scheduling cannot change identifiers, diagnostics,
// or which invalid input is reported first.
template<class Build, class Commit>
void parallel_transform_ordered_blocks(size_t item_count, size_t requested_threads,
                                       size_t block_size, Build&& build, Commit&& commit) {
    if (item_count == 0) {
        return;
    }
    if (block_size == 0) {
        throw std::runtime_error("parallel block size must be positive");
    }
    if (requested_threads == 0) {
        throw std::runtime_error("parallel worker count must be positive");
    }
    const size_t block_count = 1 + (item_count - 1) / block_size;
    const size_t worker_count = std::min(requested_threads, block_count);
    if (worker_count == 1) {
        for (size_t block = 0; block < block_count; ++block) {
            const size_t begin = block * block_size;
            const size_t end = std::min(item_count, begin + block_size);
            commit(block, begin, end, build(block, begin, end));
        }
        return;
    }

    using Result = std::decay_t<std::invoke_result_t<Build&, size_t, size_t, size_t>>;
    struct ResultSlot {
        std::optional<Result> result;
        std::exception_ptr failure;
        size_t ordinal = std::numeric_limits<size_t>::max();
        bool ready = false;
    };
    std::mutex scheduler_mutex;
    std::condition_variable scheduler_cv;
    size_t next_block = 0;
    size_t next_commit = 0;
    bool stop = false;
    const size_t result_window = std::min(
        block_count,
        worker_count > std::numeric_limits<size_t>::max() / 2
            ? block_count
            : worker_count * 2);
    std::vector<ResultSlot> slots(result_window);

    auto worker = [&]() {
        while (true) {
            size_t block = 0;
            {
                std::unique_lock<std::mutex> lock(scheduler_mutex);
                scheduler_cv.wait(lock, [&] {
                    return stop || next_block >= block_count ||
                           next_block - next_commit < result_window;
                });
                if (stop || next_block >= block_count) {
                    return;
                }
                block = next_block++;
                ResultSlot& slot = slots[block % result_window];
                slot.result.reset();
                slot.failure = nullptr;
                slot.ordinal = block;
                slot.ready = false;
            }
            const size_t begin = block * block_size;
            const size_t end = std::min(item_count, begin + block_size);
            std::optional<Result> result;
            std::exception_ptr failure;
            try {
                result.emplace(build(block, begin, end));
            } catch (...) {
                failure = std::current_exception();
            }
            {
                std::lock_guard<std::mutex> lock(scheduler_mutex);
                ResultSlot& slot = slots[block % result_window];
                if (result.has_value()) {
                    slot.result.emplace(std::move(*result));
                }
                slot.failure = failure;
                slot.ready = true;
            }
            scheduler_cv.notify_all();
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    try {
        for (size_t i = 0; i < worker_count; ++i) {
            workers.emplace_back(worker);
        }
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(scheduler_mutex);
            stop = true;
        }
        scheduler_cv.notify_all();
        for (std::thread& thread : workers) {
            thread.join();
        }
        throw;
    }

    std::exception_ptr ordered_failure;
    for (size_t block = 0; block < block_count; ++block) {
        std::optional<Result> result;
        {
            std::unique_lock<std::mutex> lock(scheduler_mutex);
            scheduler_cv.wait(lock, [&] {
                const ResultSlot& slot = slots[block % result_window];
                return slot.ordinal == block && slot.ready;
            });
            ResultSlot& slot = slots[block % result_window];
            if (slot.failure) {
                ordered_failure = slot.failure;
            } else {
                result.emplace(std::move(*slot.result));
                slot.result.reset();
            }
        }
        if (ordered_failure) {
            break;
        }
        const size_t begin = block * block_size;
        const size_t end = std::min(item_count, begin + block_size);
        try {
            commit(block, begin, end, std::move(*result));
        } catch (...) {
            ordered_failure = std::current_exception();
            break;
        }
        {
            std::lock_guard<std::mutex> lock(scheduler_mutex);
            next_commit = block + 1;
        }
        scheduler_cv.notify_all();
    }
    if (ordered_failure) {
        std::lock_guard<std::mutex> lock(scheduler_mutex);
        stop = true;
    }
    scheduler_cv.notify_all();
    for (std::thread& thread : workers) {
        thread.join();
    }
    if (ordered_failure) {
        std::rethrow_exception(ordered_failure);
    }
}

// A lazy cache remains demand-driven, but unrelated keys no longer serialize behind one global
// miss lock. Construction is single-flight within a shard. Values live in separately allocated
// immutable objects, so callers may retain the returned reference while a shard's map rehashes.
template<class Key, class Value, size_t ShardCount = 256>
class ShardedLazyCache {
public:
    explicit ShardedLazyCache(bool concurrent) : concurrent_(concurrent) {
        static_assert(ShardCount > 0);
    }

    template<class Builder>
    const Value& get_or_build(const Key& key, Builder&& builder) {
        Shard& shard = shards_[std::hash<Key>{}(key) % ShardCount];
        if (!concurrent_) {
            auto cached = shard.values.find(key);
            if (cached == shard.values.end()) {
                cached = shard.values.emplace(
                    key, std::make_unique<const Value>(builder())).first;
                build_count_.fetch_add(1, std::memory_order_relaxed);
            }
            return *cached->second;
        }
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            const auto cached = shard.values.find(key);
            if (cached != shard.values.end()) {
                return *cached->second;
            }
        }
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto cached = shard.values.find(key);
        if (cached == shard.values.end()) {
            cached = shard.values.emplace(
                key, std::make_unique<const Value>(builder())).first;
            build_count_.fetch_add(1, std::memory_order_relaxed);
        }
        return *cached->second;
    }

    size_t build_count() const {
        return build_count_.load(std::memory_order_relaxed);
    }

private:
    struct Shard {
        std::shared_mutex mutex;
        std::unordered_map<Key, std::unique_ptr<const Value>> values;
    };

    bool concurrent_;
    std::atomic<size_t> build_count_{0};
    std::array<Shard, ShardCount> shards_;
};

int run_annotation_index(int argc, char** argv) {
    using namespace pancollapse::direct_count;
    bool direct_facts = false;
    for (int i = 2; i < argc; ++i) {
        const std::string_view key = argv[i];
        direct_facts |= key == "--xg";
    }
    if (direct_facts) {
        pancollapse::direct_index::BuildOptions options;
        options.memory_limit_bytes = 300000000000ULL;
        std::filesystem::path receipt;
        std::set<std::string> seen;
        for (int i = 2; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help" || key == "-h") { std::cout << kUsageText << '\n'; return 0; }
            if (!seen.insert(key).second || i + 1 == argc)
                throw std::runtime_error("index requires unique options with values");
            const std::string value = argv[++i];
            if (key == "--from-annotation-index") options.source_index = value;
            else if (key == "--xg") options.xg = value;
            else if (key == "--gbwt") options.gbwt = value;
            else if (key == "--r-index") options.r_index = value;
            else if (key == "--r-index-manifest") receipt = value;
            else if (key == "--out") options.output = value;
            else if (key == "--threads") options.threads = parse_size_option(key, value);
            else if (key == "--memory-budget") options.memory_limit_bytes = pancollapse::count_memory::parse_bytes(value);
            else throw std::runtime_error("unsupported direct index option: " + key);
        }
        if (options.source_index.empty() || options.xg.empty() || options.gbwt.empty() ||
            options.r_index.empty() || options.output.empty() || receipt.empty() || options.threads == 0)
            throw std::runtime_error("direct index requires --from-annotation-index, --xg, --gbwt, --r-index, --r-index-manifest, --out, and positive --threads");
        if (std::filesystem::exists(options.output)) throw std::runtime_error("index destination already exists");
        if (!pancollapse::native_count::is_index(options.source_index))
            throw std::runtime_error("direct index requires a native PCANBND1 source; bind annotation facts with index --gbwt first");
        const auto binding = read_r_index_manifest(receipt);
        if (std::filesystem::file_size(options.gbwt) != binding.gbwt_size_bytes ||
            std::filesystem::file_size(options.r_index) != binding.r_index_size_bytes)
            throw std::runtime_error("index GBWT/RI byte sizes disagree with the binding receipt");
        std::ostringstream provenance;
        provenance << "{\"schema\":\"pancollapse-direct-source-v1\",\"annotation_index\":"
            << reference_json_string(options.source_index.string())
            << ",\"xg\":" << reference_json_string(options.xg.string())
            << ",\"xg_size_bytes\":" << std::filesystem::file_size(options.xg)
            << ",\"gbwt\":" << reference_json_string(options.gbwt.string())
            << ",\"gbwt_size_bytes\":" << binding.gbwt_size_bytes
            << ",\"gbwt_receipt_sha256\":\"" << binding.gbwt_sha256
            << "\",\"r_index\":" << reference_json_string(options.r_index.string())
            << ",\"r_index_size_bytes\":" << binding.r_index_size_bytes
            << ",\"r_index_receipt_sha256\":\"" << binding.r_index_sha256
            << "\",\"source_hashes_verified\":false}";
        options.provenance_json = provenance.str();
        pancollapse::direct_index::build(options);
        return 0;
    }
    bool graph_specific = false;
    for (int i=2;i<argc;++i) {
        const std::string_view key=argv[i];
        graph_specific |= key=="--xg"||key=="--gbz"||key=="--gbwt";
    }
    if (graph_specific) {
        pancollapse::native_count::BuildOptions options;
        std::set<std::string> seen;
        for(int i=2;i<argc;++i) {
            const std::string key=argv[i];
            if(key=="--help"||key=="-h") {std::cout<<kUsageText<<'\n';return 0;}
            if(!seen.insert(key).second||i+1==argc)throw std::runtime_error("index requires unique options with values");
            const std::string value=argv[++i];
            if(key=="--gbwt")options.gbwt=value;
            else if(key=="--from-annotation-index")options.source_index=value;
            else if(key=="--count-bundle")options.count_bundle=value;
            else if(key=="--out")options.output=value;
            else if(key=="--threads")options.threads=parse_size_option(key,value);
            else throw std::runtime_error("unsupported prepared index option: "+key);
        }
        pancollapse::native_count::build(options);
        return 0;
    }
    std::filesystem::path annotation, reference, ledger_path, output, previous_index;
    size_t threads = 1;
    AnnotationFormat format = AnnotationFormat::gff3, reference_format = AnnotationFormat::gff3;
    std::set<std::string> seen;
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help" || key == "-h") { std::cout << kUsageText << '\n'; return 0; }
        if (!seen.insert(key).second || i + 1 == argc)
            throw std::runtime_error("index requires unique options with values");
        const std::string value = argv[++i];
        if (key == "--annotation") annotation = value;
        else if (key == "--annotation-reference") reference = value;
        else if (key == "--path-identity-ledger") ledger_path = value;
        else if (key == "--out") output = value;
        else if (key == "--from-annotation-index") previous_index = value;
        else if (key == "--threads") threads = parse_size_option(key, value);
        else if (key == "--annotation-format") format = parse_annotation_format(value);
        else if (key == "--annotation-reference-format") reference_format = parse_annotation_format(value);
        else throw std::runtime_error("unsupported index option: " + key);
    }
    if (ledger_path.empty() || output.empty() || threads == 0 ||
        (previous_index.empty() && (annotation.empty() || reference.empty())) ||
        (!previous_index.empty() && (!annotation.empty() || !reference.empty() ||
          format != AnnotationFormat::gff3 || reference_format != AnnotationFormat::gff3)))
        throw std::runtime_error("index requires --path-identity-ledger and --out, plus either --annotation/--annotation-reference or --from-annotation-index (PCANIDX2)");
    if (std::filesystem::exists(output)) throw std::runtime_error("index destination already exists");
    const auto started = std::chrono::steady_clock::now();
    auto indexed = read_index_ledger(ledger_path, [&](size_t rows, size_t, uint64_t bytes) {
        std::cerr << "panCollapse: indexing: phase=identity_ledger rows=" << rows << " bytes=" << bytes << '\n';
    });
    std::cerr << "panCollapse: indexing: phase=" << (previous_index.empty() ? "annotation_compile" : "existing_facts_load") << '\n';
    auto facts = previous_index.empty()
        ? CountFactCatalog::from_annotation(annotation, reference, indexed.ledger,
            {ProfileId::cr7_v1, ProfileId::pansc_strict_v1}, format, reference_format, indexed.binding.ledger_sha256)
        : CountFactCatalog::load_annotation_index(previous_index, indexed.ledger, indexed.binding,
            {ProfileId::cr7_v1, ProfileId::pansc_strict_v1});
    std::cerr << "panCollapse: indexing: phase=binary_write parents=" << facts.parents().size() << '\n';
    const int previous_threads = omp_get_max_threads();
    omp_set_num_threads(static_cast<int>(std::min<size_t>(threads, INT_MAX)));
    try { facts.write_prepared_annotation_index(output, indexed.ledger, indexed.binding); }
    catch (...) { omp_set_num_threads(previous_threads); throw; }
    omp_set_num_threads(previous_threads);
    std::cout << "{\"schema\":\"pancollapse-annotation-index-v3\",\"status\":\"complete\","
              << "\"parents\":" << facts.parents().size() << ",\"genes\":" << facts.genes().size()
              << ",\"size_bytes\":" << std::filesystem::file_size(output)
              << ",\"seconds\":" << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()
              << ",\"fact_content_id\":\"" << facts.content_id() << "\"}\n";
    return 0;
}

int run_native_count(int argc, char** argv) {
    Options options = parse_native_count_options(argc, argv);
    pancollapse::prepared_count::Config config;
    config.index = options.annotation_index;
    config.gamp = options.gamp;
    config.whitelist = options.barcode_whitelist;
    config.outdir = options.out_dir;
    config.profiles = options.count_profiles;
    config.threads = options.threads;
    config.cb_length = options.raw_cb_length;
    config.umi_length = options.raw_umi_length;
    config.max_alignments_per_read = options.max_alignments_per_read;
    config.write_10x_mex = options.count_10x_mex;
    config.preload_only = options.count_preload_only;
    config.fail_molecule_identity =
        options.molecule_identity_failures == MoleculeIdentityFailurePolicy::Fail;
    config.version = PANCOLLAPSE_VERSION;
    config.analysis_scope = options.analysis_scope;
    config.memory = options.memory;
    for (int i = 0; i < argc; ++i) {
        if (i != 0) config.command_line += ' ';
        config.command_line += argv[i];
    }
    return pancollapse::prepared_count::run(config);
}

int run_convert(int argc, char** argv) {
    const auto invocation_started = std::chrono::steady_clock::now();
    Options options = parse_options(argc, argv);
    const bool production_identity = !options.path_identity_ledger.empty();
    const auto metadata_started = std::chrono::steady_clock::now();
    const std::unordered_set<std::string> strict_allowlisted_parents =
        options.strict_allowlisted_parents.empty()
            ? std::unordered_set<std::string>{}
            : read_strict_allowlisted_parents(options.strict_allowlisted_parents);
    // Production uses one strict, versioned identity ledger. The hst-v1 adapter below is the only
    // route to historical 2/3-column t2g behavior; CLI parsing makes the two routes exclusive.
    std::optional<path_identity::PathIdentityLedger> identity_ledger;
    T2gData t2g;
    BodyT2gData body_t2g;
    pancollapse::GraphInput graph_input;
    if (production_identity) {
        const auto ledger_started = std::chrono::steady_clock::now();
        const auto ledger_bytes = std::filesystem::file_size(options.path_identity_ledger);
        std::cerr << "panCollapse: initialization: phase=identity_ledger_begin bytes="
                  << ledger_bytes << '\n';
        const auto ledger_progress = [&](size_t rows, size_t parents, uint64_t bytes) {
            std::cerr << "panCollapse: initialization: phase=identity_ledger_load rows="
                      << rows << " parents=" << parents << " bytes=" << bytes
                      << " total_bytes=" << ledger_bytes << " seconds="
                      << std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - ledger_started).count()
                      << '\n';
        };
        identity_ledger = path_identity::read(
            options.path_identity_ledger,
            options.count_mode == pathtally::CountMode::GeneFullEx50pAS,
            ledger_progress);
        std::cerr << "panCollapse: initialization: phase=identity_ledger_validated rows="
                  << identity_ledger->rows_by_path.size()
                  << " qualified_targets=" << identity_ledger->qualified_target_count() << " seconds="
                  << std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - ledger_started).count() << '\n';
        if (!strict_allowlisted_parents.empty()) {
            for (const std::string& parent : strict_allowlisted_parents) {
                if (identity_ledger->identities_by_parent.count(parent) == 0) {
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
                // Score mode resolves raw exon paths after tallying. Production count modes keep
                // path identity numeric and need only the canonical target dictionary.
                if (options.count_mode == pathtally::CountMode::Score) {
                    t2g.path_transcript.emplace(
                        raw_path, identity_ledger->target_transcript(annotation));
                    t2g.path_unique_parent.emplace(raw_path, annotation.unique_parent);
                }
                const auto& target = identity_ledger->target_transcript(annotation);
                const auto [bound, inserted] =
                    t2g.transcript_gene.emplace(target, annotation.gene_id);
                if (!inserted && bound->second != annotation.gene_id) {
                    throw std::runtime_error(
                        "internal transcript target maps to multiple genes: " + target);
                }
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

    const double metadata_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - metadata_started).count();
    std::cerr << "panCollapse: initialization: phase=metadata_parse production_rows="
              << (identity_ledger.has_value() ? identity_ledger->rows_by_path.size() : 0)
              << " seconds=" << metadata_seconds << '\n';

    auto load_graph_file = [&](const std::filesystem::path& path, const char* role,
                               auto load) {
        const auto started = std::chrono::steady_clock::now();
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error(
                std::string("cannot open ") + role + ": " + path.string());
        }
        load(input);
        std::cerr << "panCollapse: initialization: phase=" << role
                  << "_deserialize seconds="
                  << std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - started).count()
                  << '\n';
    };
    if (!options.gbz.empty()) {
        load_graph_file(options.gbz, "gbz",
                        [&](std::istream& in) { graph_input.load_gbz(in); });
    } else {
        load_graph_file(options.xg, "xg",
                        [&](std::istream& in) { graph_input.load_xg(in); });
        if (!options.gbwt.empty()) {
            load_graph_file(options.gbwt, "gbwt",
                            [&](std::istream& in) { graph_input.load_gbwt(in); });
        }
    }
    graph_input.initialize_gbwt(options.threads);
    const auto& graph = graph_input.nodes();
    if (graph_input.uses_gbwt()) {
        const auto started = std::chrono::steady_clock::now();
        std::vector<handlegraph::path_handle_t> selected_paths;
        std::unordered_set<std::string> selected_names;
        graph_input.for_each_path_handle([&](handlegraph::path_handle_t path) {
            const auto name = graph_input.get_path_name(path);
            const bool selected = production_identity
                ? identity_ledger->rows_by_path.count(name) != 0
                : (resolve_graph_transcript(t2g, name, options.count_mode != pathtally::CountMode::Score) != nullptr ||
                   (options.count_mode != pathtally::CountMode::Score && resolve_body_graph_path(body_t2g, name) != nullptr));
            if (selected) {
                if (production_identity) {
                    auto& row = identity_ledger->rows_by_path.at(name);
                    if (row.graph_bound) {
                        throw std::runtime_error("ambiguous GBWT metadata path name: " + name);
                    }
                    row.graph_bound = true;
                    row.graph_handle = handlegraph::as_integer(path);
                } else if (!selected_names.insert(name).second) {
                    throw std::runtime_error("ambiguous GBWT metadata path name: " + name);
                }
                selected_paths.push_back(path);
            }
        });
        graph_input.prepare_paths(selected_paths, options.threads);
        std::cerr << "panCollapse: initialization: phase=selected_path_nodes paths=" << selected_paths.size()
                  << " seconds="
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() << '\n';
    }

    const auto ledger_path_handles_started = std::chrono::steady_clock::now();
    if (production_identity) {
        // Resolve handles by enumerating the graph's paths once rather than asking for each
        // ledger row by name. has_path and get_path_handle are each a locate on the XG's
        // path-name index, so the by-name form paid two of them per row across a ledger with
        // millions of rows. Enumeration visits every path once and costs a ledger lookup per
        // path instead.
        if (!graph_input.uses_gbwt()) graph_input.for_each_path_handle([&](const handlegraph::path_handle_t& path) {
            std::string path_name = graph_input.get_path_name(path);
            const auto found = identity_ledger->rows_by_path.find(path_name);
            if (found == identity_ledger->rows_by_path.end()) {
                return;
            }
            if (!found->second.graph_bound) {
                found->second.graph_handle = handlegraph::as_integer(path);
                found->second.graph_bound = true;
            }
        });
        // Validate in ledger order, which is sorted by path name, so the first failure
        // reported stays a property of the input rather than of graph enumeration order.
        for (const auto& entry : identity_ledger->rows_by_path) {
            const std::string& path_name = entry.first;
            if (!entry.second.graph_bound) {
                throw std::runtime_error("path identity ledger vg_path_name " + path_name +
                                         " is absent from the graph paths");
            }
            const uint64_t graph_length = graph_input.get_path_length(
                handlegraph::as_path_handle(entry.second.graph_handle));
            if (graph_length != entry.second.vg_path_length) {
                throw std::runtime_error("path identity ledger vg_path_length for " + path_name +
                                         " is " +
                                         std::to_string(entry.second.vg_path_length) +
                                         " but the graph path length is " +
                                         std::to_string(graph_length));
            }
        }
    }
    if (production_identity) {
        const double ledger_path_handles_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - ledger_path_handles_started).count();
        std::cerr << "panCollapse: initialization: phase=ledger_path_handles paths="
                  << identity_ledger->rows_by_path.size()
                  << " seconds=" << ledger_path_handles_seconds << '\n';
    }

    // Score-mode HST lookup (node -> HST names, cached per node). Ledger-mode lookup is below.
    // Only the one for the active mode is built. Both do the node-id-space validation on first
    // touch.
    std::unordered_map<uint64_t, std::string> hst_path_name;
    ShardedLazyCache<int64_t, std::vector<std::pair<const std::string*, bool>>>
        node_hst_cache(options.threads > 1);
    pathtally::PathLookup lookup;

    // Ledger-mode state. ledger_path_info covers every exon/body reference path by graph handle.
    // The by-name maps collapse the score tally after each read: exon paths always collapse to a
    // transcript target; legacy two-column bodies collapse to a gene, while D063 three-column
    // bodies collapse to that same canonical transcript target.
    struct PathInfo {
        // Production names remain owned by the immutable identity ledger. Legacy adapters
        // retain an owned copy because their parser has no stable row object.
        std::unique_ptr<const std::string> legacy_name;
        uint32_t gene_idx = 0;  // gene index (legacy geometry/orientation and BAM header)
        bool is_exon = false;   // spliced exon path vs unspliced body path
        uint32_t target_id = 0;
        uint32_t parent_rank = 0;
        const path_identity::PathIdentityRow* identity = nullptr;
        bool body_resolvable = true;

        const std::string& path_name() const {
            if (identity != nullptr) return identity->vg_path_name.str();
            if (legacy_name) return *legacy_name;
            throw std::logic_error("path catalog lacks its identity");
        }
    };
    std::unordered_map<uint64_t, PathInfo> ledger_path_info;
    std::unordered_map<std::string, uint32_t> exon_name_transcript;  // exon path name -> transcript target id
    std::unordered_map<std::string, uint32_t> body_name_gene;        // legacy body path -> gene idx
    std::unordered_map<std::string, uint32_t> body_name_transcript;  // D063 body path -> target id
    // The validated Parent index is already lexical and compact. Reuse its rank
    // instead of retaining another per-Parent hash table and pointer vector.
    const auto ledger_parent_rank = [&](std::string_view parent) {
        return static_cast<uint32_t>(identity_ledger->identities_by_parent.index_of(parent));
    };
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
    // A production conversion BAM is a typed union: ordinary Gene rows and exact Ex50 rows are
    // computed together from the same GAMP regardless of which downstream count mode selected it.
    // Legacy and score BAMs deliberately keep their marker-absent historical interpretation.
    const bool exact_ex50 = production_identity &&
                           options.count_mode != pathtally::CountMode::Score &&
                           !options.bam_out.empty();
    using ExactEdge = std::tuple<int64_t, bool, int64_t, bool>;
    enum class ExactOrientationBasis : uint8_t {
        SharedSame,
        SharedOpposite,
        IntrinsicBody,
    };
    struct ExactEx50Model {
        const path_identity::PathIdentityRow* exon = nullptr;
        const path_identity::PathIdentityRow* body = nullptr;
        uint64_t exon_path_handle = 0;
        uint64_t body_path_handle = 0;
        uint32_t target_id = 0;
        uint32_t exon_path_rank = 0;
        uint32_t body_path_rank = 0;
        uint32_t exon_parent_rank = 0;
        uint32_t body_parent_rank = 0;
        // A sorted unique vector, not a set: these geometries are built once and then only
        // membership-tested, and a red-black node costs about 80 bytes to carry 32 bytes of
        // edge. Over 1.23 million geometries that difference was 22.7 GiB of a 122 GiB peak.
        std::shared_ptr<const std::vector<ExactEdge>> exon_edges;
        ExactOrientationBasis orientation_basis = ExactOrientationBasis::SharedSame;
        std::shared_ptr<const std::vector<ExactEdge>> body_edges;
    };
    std::vector<ExactEx50Model> exact_ex50_models;
    std::unordered_map<uint64_t, std::vector<size_t>> exact_ex50_models_by_body_path;
    size_t exact_exon_edge_geometry_count = 0;
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
        struct ReferenceStep {
            uint64_t path_handle = 0;
            const std::string* path_name = nullptr;
            bool path_is_reverse = false;
        };
        std::vector<ReferenceStep> ref_paths;
        struct LegacyMetadata {
            std::vector<std::pair<uint32_t, bool>> gene_orient;
            std::vector<uint32_t> body_genes;
        };
        std::optional<LegacyMetadata> legacy;
        struct ExactStep {
            uint64_t path_handle = 0;
            bool path_is_reverse = false;
        };
        std::vector<ExactStep> exact_exon_steps;
        std::vector<ExactStep> exact_body_steps;
    };
    pancollapse::BoundedReferenceCache<int64_t, NodeLedger> node_ledger_cache(
        std::numeric_limits<size_t>::max(),
        [](const NodeLedger& node) {
            size_t bytes = sizeof(node);
            bytes += node.ref_paths.capacity() * sizeof(NodeLedger::ReferenceStep);
            bytes += node.exact_exon_steps.capacity() * sizeof(NodeLedger::ExactStep);
            bytes += node.exact_body_steps.capacity() * sizeof(NodeLedger::ExactStep);
            if (node.legacy.has_value()) {
                bytes += node.legacy->gene_orient.capacity() *
                         sizeof(std::pair<uint32_t, bool>);
                bytes += node.legacy->body_genes.capacity() * sizeof(uint32_t);
            }
            return bytes;
        });
    using ReadNodeCache = pathtally::ReadNodeTallyCache<NodeLedger>;
    using NodeLedgerLease = std::shared_ptr<const NodeLedger>;
    std::function<NodeLedgerLease(int64_t)> node_ledger_of;
    pathtally::PathLookup ledger_lookup;

    const auto path_catalog_started = std::chrono::steady_clock::now();
    if (production_identity) {
        const size_t ledger_rows = identity_ledger->rows_by_path.size();
        ledger_path_info.reserve(ledger_rows);
        const auto& parent_identities = identity_ledger->identities_by_parent;
        if (parent_identities.size() >
            static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error(
                "path identity ledger has too many unique Parents for 32-bit internal ranks");
        }
    }
    if (options.count_mode == pathtally::CountMode::Score) {
        graph_input.for_each_path_handle([&](const handlegraph::path_handle_t& path) {
            std::string name = graph_input.get_path_name(path);
            if (resolve_graph_transcript(t2g, name, false) != nullptr) {
                hst_path_name.emplace(handlegraph::as_integer(path), std::move(name));
            }
        });
        lookup = [&](int64_t node_id, const std::function<void(const std::string&, bool)>& emit) {
            const auto& entries = node_hst_cache.get_or_build(node_id, [&]() {
                if (!graph.has_node(node_id)) {
                    throw std::runtime_error(
                        "GAMP/graph node-id-space mismatch: node " + std::to_string(node_id) +
                        " is absent from the graph; the GAMP was likely aligned to a different graph");
                }
                std::vector<std::pair<const std::string*, bool>> built;
                graph_input.for_each_node_occurrence(node_id, [&](const pancollapse::GraphPathStep& step) {
                    const auto it =
                        hst_path_name.find(handlegraph::as_integer(step.path));
                    if (it != hst_path_name.end()) {
                        built.emplace_back(&it->second,
                                           graph.get_is_reverse(step.handle));
                    }
                    return true;
                });
                return built;
            });
            for (const auto& [name_ptr, path_is_reverse] : entries) {
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
            PathInfo path_info;
            path_info.gene_idx = gene_idx;
            path_info.is_exon = true;
            if (production_identity) {
                const auto& row = std::as_const(identity_ledger->rows_by_path).at(name);
                path_info.target_id = target_id;
                path_info.parent_rank =
                    ledger_parent_rank(row.annotation.unique_parent);
                path_info.identity = &row;
            } else {
                path_info.legacy_name = std::make_unique<const std::string>(name);
            }
            ledger_path_info.emplace(handlegraph::as_integer(path), std::move(path_info));
            if (!production_identity) {
                exon_name_transcript.emplace(name, target_id);
            }
            if (!body_t2g.transcript_specific) gene_exon_paths[gene_idx].push_back(path);
            transcript_exon_paths[target_id].push_back(path);
        };

        if (production_identity) {
            // Validation above already resolved every ledger path to an XG handle. Reuse those
            // handles instead of scanning every path in the graph and repeating string lookups.
            // Handle order reproduces the relevant-path order of for_each_path_handle, preserving
            // deterministic vector contents and invalid-input diagnostics.
            std::vector<std::pair<uint64_t, const path_identity::PathIdentityRow*>> catalog_rows;
            catalog_rows.reserve(identity_ledger->rows_by_path.size());
            for (const auto& [name, row] : identity_ledger->rows_by_path) {
                catalog_rows.emplace_back(row.graph_handle, &row);
            }
            std::sort(catalog_rows.begin(), catalog_rows.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& [path_integer, row] : catalog_rows) {
                const handlegraph::path_handle_t path =
                    handlegraph::as_path_handle(path_integer);
                const std::string& name = row->vg_path_name;
                const auto& annotation = row->annotation;
                if (annotation.feature_layer == "exon") {
                    record_exon_path(path, name, identity_ledger->target_transcript(annotation));
                    continue;
                }
                const uint32_t gene_idx = gene_index_for(annotation.gene_id);
                const uint32_t target_id = t2g.target_ids.at(identity_ledger->target_transcript(annotation));
                ledger_path_info.emplace(
                    path_integer,
                    PathInfo{nullptr, gene_idx, false, target_id,
                             ledger_parent_rank(annotation.unique_parent),
                             row, true});
                transcript_body_paths[target_id].push_back(path);
            }
        } else {
            graph_input.for_each_path_handle([&](const handlegraph::path_handle_t& path) {
                const std::string name = graph_input.get_path_name(path);
                // Exact exon rows win first. Then exact body rows win over the legacy exon
                // bare-name fallback, so an explicitly named transcript-body path cannot be
                // mistaken for an exon.
                const std::string* transcript = resolve_graph_transcript(t2g, name, false);
                if (transcript != nullptr) {
                    record_exon_path(path, name, *transcript);
                    return;
                }
                const BodyPathTarget* body = resolve_body_graph_path(body_t2g, name);
                if (body != nullptr) {
                    const uint32_t gene_idx = gene_index_for(body->gene);
                    if (body_t2g.transcript_specific) {
                        const uint32_t target_id = t2g.target_ids.at(body->transcript);
                        ledger_path_info.emplace(handlegraph::as_integer(path),
                                                 PathInfo{std::make_unique<const std::string>(name), gene_idx, false});
                        body_name_transcript.emplace(name, target_id);
                        transcript_body_paths[target_id].push_back(path);
                    } else {
                        ledger_path_info.emplace(handlegraph::as_integer(path),
                                                 PathInfo{std::make_unique<const std::string>(name), gene_idx, false});
                        body_name_gene.emplace(name, gene_idx);
                        gene_body_paths[gene_idx].push_back(path);
                    }
                    return;
                }
                transcript = resolve_graph_transcript(t2g, name, true);
                if (transcript != nullptr) {
                    record_exon_path(path, name, *transcript);
                }
            });
        }

        const double path_catalog_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - path_catalog_started).count();
        std::cerr << "panCollapse: initialization: phase=path_catalog production="
                  << (production_identity ? 1 : 0)
                  << " relevant_paths=" << ledger_path_info.size()
                  << " seconds=" << path_catalog_seconds << '\n';

        if (exact_ex50) {
            const auto parent_geometry_started = std::chrono::steady_clock::now();
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
            struct ParentTask {
                const std::string* exon_parent = nullptr;
                const std::vector<const path_identity::PathIdentityRow*>* exon_rows = nullptr;
                const std::vector<const path_identity::PathIdentityRow*>* body_rows = nullptr;
            };
            std::vector<ParentTask> parent_tasks;
            parent_tasks.reserve(exon_rows_by_parent.size());
            for (const auto& [exon_parent, exon_rows] : exon_rows_by_parent) {
                const auto bodies = body_rows_by_exon_parent.find(exon_parent);
                if (bodies == body_rows_by_exon_parent.end() || bodies->second.empty()) {
                    throw std::runtime_error(
                        "genefull_ex50pas requires a linked body path for exon Parent " +
                        exon_parent);
                }
                parent_tasks.push_back({&exon_parent, &exon_rows, &bodies->second});
            }

            // The loader assigns lexical ranks without a second per-path hash table.
            // Keep the original exact-model bound; legacy modes need no row-rank limit.
            if (identity_ledger->rows_by_path.size() >
                static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
                throw std::runtime_error(
                    "path identity ledger has too many paths for 32-bit internal ranks");
            }

            struct ParentBlockResult {
                std::vector<ExactEx50Model> models;
                std::vector<std::string> unresolvable_exon_parents;
                std::vector<std::string> unresolvable_paths;
                std::vector<uint32_t> unresolvable_targets;
                size_t prepared_exon_paths = 0;
                size_t prepared_body_paths = 0;
            };
            // Keep enough dynamic blocks to absorb unequal Parent costs while capping scheduler
            // overhead on whole-pangenome ledgers. A desired granularity of four blocks per
            // requested worker also lets a chr20/21/22-sized run use a large host rather than
            // stopping at one worker per fixed 32-Parent block.
            constexpr size_t kMaxParentBlockSize = 32;
            constexpr size_t kTargetBlocksPerWorker = 4;
            const size_t desired_parent_blocks = parent_tasks.empty()
                ? 0
                : std::min(parent_tasks.size(),
                           options.threads > std::numeric_limits<size_t>::max() /
                                                     kTargetBlocksPerWorker
                               ? parent_tasks.size()
                               : options.threads * kTargetBlocksPerWorker);
            const size_t parent_block_size = parent_tasks.empty()
                ? 1
                : std::min(kMaxParentBlockSize,
                           1 + (parent_tasks.size() - 1) / desired_parent_blocks);
            const size_t parent_block_count = parent_tasks.empty()
                ? 0 : 1 + (parent_tasks.size() - 1) / parent_block_size;
            const size_t parent_worker_count =
                std::min(options.threads, parent_block_count);
            size_t prepared_exon_path_count = 0;
            size_t prepared_body_path_count = 0;
            size_t intrinsic_body_model_count = 0;
            std::unordered_set<const std::vector<ExactEdge>*> retained_exon_edge_geometries;
            parallel_transform_ordered_blocks(
                parent_tasks.size(), options.threads, parent_block_size,
                [&](size_t block, size_t begin, size_t end) {
                    static_cast<void>(block);
                    ParentBlockResult result;
                    for (size_t task_index = begin; task_index < end; ++task_index) {
                        const ParentTask& task = parent_tasks[task_index];
                        const std::string& exon_parent = *task.exon_parent;
                        bool parent_body_unresolvable = false;
                        std::vector<ExactEx50Model> parent_models;
                        std::vector<std::string> parent_unresolvable_paths;
                        std::vector<uint32_t> parent_unresolvable_targets;
                        struct PreparedExon {
                            const path_identity::PathIdentityRow* row = nullptr;
                            uint64_t path_handle = 0;
                            std::vector<std::pair<int64_t, bool>> steps;
                            std::shared_ptr<const std::vector<ExactEdge>> edges;
                        };
                        struct PreparedBody {
                            const path_identity::PathIdentityRow* row = nullptr;
                            uint64_t path_handle = 0;
                            std::shared_ptr<const std::vector<ExactEdge>> edges;
                        };
                        std::vector<PreparedExon> prepared_exons;
                        prepared_exons.reserve(task.exon_rows->size());
                        // Hashes only select exact-comparison buckets. Equal hashes never merge
                        // unequal exon paths, while exact duplicate geometry shares one immutable
                        // splice-edge set across every model for this Parent.
                        std::unordered_map<size_t, std::vector<size_t>> exon_geometry_buckets;
                        for (const path_identity::PathIdentityRow* exon : *task.exon_rows) {
                            PreparedExon prepared;
                            prepared.row = exon;
                            prepared.path_handle = exon->graph_handle;
                            graph_input.for_each_path_step(
                                handlegraph::as_path_handle(prepared.path_handle),
                                [&](const pancollapse::GraphPathStep& step) {
                                    const handlegraph::handle_t handle =
                                        step.handle;
                                    prepared.steps.emplace_back(graph.get_id(handle),
                                                                graph.get_is_reverse(handle));
                                });
                            prepared_exons.push_back(std::move(prepared));
                            PreparedExon& current = prepared_exons.back();
                            size_t geometry_hash = 0;
                            for (const auto& [node_id, is_reverse] : current.steps) {
                                const size_t node_hash = std::hash<int64_t>{}(node_id);
                                geometry_hash ^= node_hash + 0x9e3779b9 +
                                                 (geometry_hash << 6) + (geometry_hash >> 2);
                                const size_t reverse_hash = std::hash<bool>{}(is_reverse);
                                geometry_hash ^= reverse_hash + 0x9e3779b9 +
                                                 (geometry_hash << 6) + (geometry_hash >> 2);
                            }
                            std::vector<size_t>& representatives =
                                exon_geometry_buckets[geometry_hash];
                            for (const size_t representative : representatives) {
                                if (prepared_exons[representative].steps == current.steps) {
                                    current.edges = prepared_exons[representative].edges;
                                    break;
                                }
                            }
                            if (!current.edges) {
                                std::vector<ExactEdge> edges;
                                edges.reserve(current.steps.size() * 2);
                                for (size_t step_index = 1;
                                     step_index < current.steps.size(); ++step_index) {
                                    const auto& previous = current.steps[step_index - 1];
                                    const auto& next = current.steps[step_index];
                                    edges.emplace_back(previous.first, previous.second,
                                                       next.first, next.second);
                                    // Reverse traversal reverses node order and flips handles.
                                    edges.emplace_back(next.first, !next.second,
                                                       previous.first, !previous.second);
                                }
                                // Match std::set's content exactly: sorted and unique.
                                std::sort(edges.begin(), edges.end());
                                edges.erase(std::unique(edges.begin(), edges.end()),
                                            edges.end());
                                edges.shrink_to_fit();
                                current.edges = std::make_shared<const std::vector<ExactEdge>>(
                                    std::move(edges));
                                representatives.push_back(prepared_exons.size() - 1);
                            }
                            ++result.prepared_exon_paths;
                        }
                        std::vector<PreparedBody> prepared_bodies;
                        prepared_bodies.reserve(task.body_rows->size());
                        for (const path_identity::PathIdentityRow* body : *task.body_rows) {
                            PreparedBody prepared;
                            prepared.row = body;
                            prepared.path_handle = body->graph_handle;
                            std::vector<ExactEdge> edges;
                            std::optional<std::pair<int64_t,bool>> previous;
                            graph_input.for_each_path_step(
                                handlegraph::as_path_handle(prepared.path_handle),
                                [&](const pancollapse::GraphPathStep& step) {
                                    const auto id=graph.get_id(step.handle);
                                    const bool reverse=graph.get_is_reverse(step.handle);
                                    if(previous) {
                                        edges.emplace_back(previous->first,previous->second,id,reverse);
                                        edges.emplace_back(id,!reverse,previous->first,!previous->second);
                                    }
                                    previous=std::make_pair(id,reverse);
                                });
                            std::sort(edges.begin(),edges.end());
                            edges.erase(std::unique(edges.begin(),edges.end()),edges.end());
                            prepared.edges=std::make_shared<const std::vector<ExactEdge>>(std::move(edges));
                            prepared_bodies.push_back(std::move(prepared));
                            ++result.prepared_body_paths;
                        }

                        // Preserve the historical exon-major/body-minor pair order while reusing
                        // each path's immutable geometry. Positions only distinguish repeated body
                        // occurrences; ordinary exonic nodes need no per-node storage.
                        for (const PreparedExon& prepared_exon : prepared_exons) {
                            const path_identity::PathIdentityRow* exon = prepared_exon.row;
                            for (const PreparedBody& prepared_body : prepared_bodies) {
                                const path_identity::PathIdentityRow* body = prepared_body.row;
                                if(exon->annotation.gene_id!=body->annotation.gene_id) {
                                    throw std::runtime_error("path identity ledger body Parent "+
                                        body->annotation.unique_parent+" changes counted gene from "+
                                        exon->annotation.gene_id+" to "+body->annotation.gene_id);
                                }

                                const uint32_t target_id = std::as_const(t2g.target_ids).at(
                                    identity_ledger->target_transcript(exon->annotation));
                                parent_models.push_back(
                                    {exon, body, prepared_exon.path_handle,
                                     prepared_body.path_handle, target_id,
                                     exon->lexical_rank, body->lexical_rank,
                                     ledger_parent_rank(exon->annotation.unique_parent),
                                     ledger_parent_rank(body->annotation.unique_parent),
                                     prepared_exon.edges, ExactOrientationBasis::SharedSame,
                                     prepared_body.edges});
                            }
                        }
                        if (parent_body_unresolvable) {
                            result.unresolvable_exon_parents.push_back(exon_parent);
                            result.unresolvable_paths.insert(
                                result.unresolvable_paths.end(),
                                std::make_move_iterator(parent_unresolvable_paths.begin()),
                                std::make_move_iterator(parent_unresolvable_paths.end()));
                            result.unresolvable_targets.insert(
                                result.unresolvable_targets.end(),
                                parent_unresolvable_targets.begin(),
                                parent_unresolvable_targets.end());
                        } else {
                            result.models.insert(
                                result.models.end(),
                                std::make_move_iterator(parent_models.begin()),
                                std::make_move_iterator(parent_models.end()));
                        }
                    }
                    return result;
                },
                [&](size_t block, size_t begin, size_t end, ParentBlockResult result) {
                    static_cast<void>(block);
                    static_cast<void>(begin);
                    static_cast<void>(end);
                    // Stable block reduction preserves the old lexical-Parent/exon/body model
                    // numbering even when later blocks finish first.
                    body_unresolvable_exon_parents.insert(
                        result.unresolvable_exon_parents.begin(),
                        result.unresolvable_exon_parents.end());
                    body_unresolvable_paths.insert(result.unresolvable_paths.begin(),
                                                   result.unresolvable_paths.end());
                    body_unresolvable_targets.insert(result.unresolvable_targets.begin(),
                                                     result.unresolvable_targets.end());
                    prepared_exon_path_count += result.prepared_exon_paths;
                    prepared_body_path_count += result.prepared_body_paths;
                    for (ExactEx50Model& model : result.models) {
                        const size_t model_id = exact_ex50_models.size();
                        intrinsic_body_model_count +=
                            model.orientation_basis == ExactOrientationBasis::IntrinsicBody;
                        retained_exon_edge_geometries.insert(model.exon_edges.get());
                        exact_ex50_models_by_body_path[model.body_path_handle].push_back(model_id);
                        exact_ex50_models_by_body_path[model.exon_path_handle].push_back(model_id);
                        exact_ex50_models.push_back(std::move(model));
                    }
                });
            exact_exon_edge_geometry_count = retained_exon_edge_geometries.size();
            retained_exon_edge_geometries.clear();
            retained_exon_edge_geometries.rehash(0);
            if (!body_unresolvable_exon_parents.empty()) {
                for (auto& [path_handle, path_info] : ledger_path_info) {
                    static_cast<void>(path_handle);
                    if (!path_info.is_exon && path_info.identity != nullptr) {
                        path_info.body_resolvable =
                            body_unresolvable_exon_parents.count(
                                path_info.identity->annotation.exon_unique_parent) == 0;
                    }
                }
            }
            const double parent_geometry_seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - parent_geometry_started).count();
            std::cerr << "panCollapse: initialization: phase=exact_parent_models workers="
                      << parent_worker_count << " parents=" << parent_tasks.size()
                      << " blocks=" << parent_block_count
                      << " models=" << exact_ex50_models.size()
                      << " intrinsic_body_models=" << intrinsic_body_model_count
                      << " prepared_exon_paths=" << prepared_exon_path_count
                      << " exon_edge_geometries=" << exact_exon_edge_geometry_count
                      << " prepared_body_paths=" << prepared_body_path_count
                      << " degraded_parents=" << body_unresolvable_exon_parents.size()
                      << " seconds=" << parent_geometry_seconds << '\n';

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

        const auto splice_geometry_started = std::chrono::steady_clock::now();
        size_t d63_splice_target_edges_evaluated = 0;
        size_t d63_splice_target_edges_owned = 0;
        size_t d63_splice_target_edges_fragment_only = 0;
        size_t d63_splice_target_edges_adjacent_vetoed = 0;

        using GeometryEdge = std::pair<int64_t, int64_t>;
        struct GeometryResult {
            std::vector<std::pair<GeometryEdge, uint32_t>> splice_owners;
            std::vector<pathtally::TranscriptSpan> spans;
            size_t evaluated = 0;
            size_t owned = 0;
            size_t fragment_only = 0;
            size_t adjacent_vetoed = 0;
        };

        auto exon_target_id = [&](const handlegraph::path_handle_t& exon_path) {
            if (production_identity) {
                return std::as_const(ledger_path_info)
                    .at(handlegraph::as_integer(exon_path))
                    .target_id;
            }
            return std::as_const(exon_name_transcript).at(
                graph_input.get_path_name(exon_path));
        };

        // Shared geometry worker. Legacy D060 records exon spans against a pooled gene body. D063
        // omits spans and derives splice ownership by comparing one canonical transcript's exon
        // paths only with that transcript's own body paths.
        auto record_geometry = [&](const std::vector<handlegraph::path_handle_t>& exon_paths,
                                   const std::vector<handlegraph::path_handle_t>& body_paths,
                                   const std::optional<uint32_t>& legacy_gene_idx) {
            GeometryResult result;
            if (!legacy_gene_idx.has_value()) {
                std::unordered_set<GeometryEdge, pathtally::NodePairHash> exon_edges;
                std::optional<uint32_t> target_id;
                for (const handlegraph::path_handle_t& exon_path : exon_paths) {
                    const uint32_t path_target = exon_target_id(exon_path);
                    if (!target_id.has_value()) {
                        target_id = path_target;
                    } else if (*target_id != path_target) {
                        throw std::runtime_error(
                            "internal transcript-body geometry mixed canonical targets");
                    }
                    std::vector<int64_t> node_ids;
                    graph_input.for_each_path_step(
                        exon_path, [&](const pancollapse::GraphPathStep& step) {
                            node_ids.push_back(graph.get_id(step.handle));
                    });
                    for (size_t i = 0; i + 1 < node_ids.size(); ++i) {
                        if (node_ids[i] != node_ids[i + 1]) {
                            exon_edges.insert(
                                pathtally::undirected_node_pair(node_ids[i], node_ids[i + 1]));
                        }
                    }
                }
                if (!target_id.has_value()) {
                    return result;
                }

                // A degraded Parent cannot contribute body geometry, but its exon path remains
                // valid evidence.  Preserve its own exon edges only when no clean sibling body
                // for this canonical target remains; otherwise use only clean body geometry.
                const bool degraded_parent =
                    production_identity && body_unresolvable_targets.count(*target_id) != 0;
                // For the fragment-only audit, record whether both endpoints ever coexist on one
                // raw body path.
                pathtally::BodyGeometryIndex body_geometry;
                std::unordered_set<GeometryEdge, pathtally::NodePairHash>
                    same_path_endpoint_edges;
                bool has_usable_body_path = false;
                for (const handlegraph::path_handle_t& body_path : body_paths) {
                    if (production_identity) {
                        // ledger_path_info is already keyed by graph handle and holds a pointer
                        // to the ledger row. Going through get_path_name built a string and
                        // hashed it into the millions-of-rows by-name map once per body path.
                        const auto& annotation =
                            ledger_path_info.at(handlegraph::as_integer(body_path))
                                .identity->annotation;
                        if (body_unresolvable_exon_parents.count(
                                annotation.exon_unique_parent) != 0) {
                            continue;
                        }
                    }
                    has_usable_body_path = true;
                    std::vector<int64_t> path_node_ids;
                    graph_input.for_each_path_step(
                        body_path, [&](const pancollapse::GraphPathStep& step) {
                            path_node_ids.push_back(graph.get_id(step.handle));
                        });
                    pathtally::add_body_path(body_geometry, path_node_ids);
                    if (same_path_endpoint_edges.size() == exon_edges.size()) {
                        continue;  // all candidates already have same-path support; only union/adjacency remains
                    }
                    // Probe the small side. A body path carries on the order of a thousand
                    // nodes while a transcript contributes about a hundred candidate edges, so
                    // asking each candidate whether both its endpoints are on this path is an
                    // order of magnitude fewer lookups than walking the path's nodes against an
                    // index of the candidates. The condition is the same one: the old form
                    // indexed each edge under its lower endpoint, so it inserted exactly when
                    // the path held both endpoints.
                    std::unordered_set<int64_t> path_nodes(path_node_ids.begin(),
                                                           path_node_ids.end());
                    for (const GeometryEdge& edge : exon_edges) {
                        if (path_nodes.count(edge.first) != 0 &&
                            path_nodes.count(edge.second) != 0) {
                            same_path_endpoint_edges.insert(edge);
                        }
                    }
                }

                if (degraded_parent && !has_usable_body_path) {
                    for (const GeometryEdge& edge : exon_edges) {
                        ++result.evaluated;
                        ++result.owned;
                        result.splice_owners.emplace_back(edge, *target_id);
                    }
                    return result;
                }

                for (const GeometryEdge& edge : exon_edges) {
                    ++result.evaluated;
                    if (body_geometry.adjacent_edges.count(edge) != 0) {
                        ++result.adjacent_vetoed;
                        continue;
                    }
                    // body_geometry_owns_splice_edge re-tests the adjacency the line above just
                    // ruled out, and re-normalizes a pair that is already normalized. Both
                    // endpoint tests are all that remains of it here.
                    if (body_geometry.nodes.count(edge.first) == 0 ||
                        body_geometry.nodes.count(edge.second) == 0) {
                        continue;
                    }
                    ++result.owned;
                    if (same_path_endpoint_edges.count(edge) == 0) {
                        ++result.fragment_only;
                    }
                    result.splice_owners.emplace_back(edge, *target_id);
                }
                return result;
            }

            // Legacy D060 deliberately retains its historical numeric span/edge behavior.
            std::unordered_set<int64_t> body_nodes;
            for (const handlegraph::path_handle_t& body_path : body_paths) {
                graph_input.for_each_path_step(body_path, [&](const pancollapse::GraphPathStep& step) {
                    body_nodes.insert(graph.get_id(step.handle));
                });
            }
            std::vector<int64_t> body_sorted(body_nodes.begin(), body_nodes.end());
            std::sort(body_sorted.begin(), body_sorted.end());

            result.spans.reserve(exon_paths.size());
            for (const handlegraph::path_handle_t& exon_path : exon_paths) {
                std::vector<int64_t> node_ids;
                graph_input.for_each_path_step(exon_path, [&](const pancollapse::GraphPathStep& step) {
                    node_ids.push_back(graph.get_id(step.handle));
                });
                const uint32_t target_id = exon_target_id(exon_path);

                int64_t lo = std::numeric_limits<int64_t>::max();
                int64_t hi = std::numeric_limits<int64_t>::min();
                for (const int64_t id : node_ids) {
                    if (body_nodes.count(id) != 0) {
                        lo = std::min(lo, id);
                        hi = std::max(hi, id);
                    }
                }
                if (lo <= hi) {
                    result.spans.push_back({lo, hi, target_id});
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
                        result.splice_owners.emplace_back(edge, target_id);
                    }
                }
            }
            return result;
        };

        struct GeometryTask {
            uint32_t key = 0;
            const std::vector<handlegraph::path_handle_t>* exon_paths = nullptr;
            const std::vector<handlegraph::path_handle_t>* body_paths = nullptr;
            std::optional<uint32_t> legacy_gene_idx;
        };
        std::vector<GeometryTask> geometry_tasks;
        if (body_t2g.transcript_specific) {
            geometry_tasks.reserve(transcript_exon_paths.size());
            for (const auto& [target_id, exon_paths] : transcript_exon_paths) {
                const auto body = transcript_body_paths.find(target_id);
                geometry_tasks.push_back(
                    {target_id, &exon_paths,
                     body == transcript_body_paths.end() ? nullptr : &body->second,
                     std::nullopt});
            }
        } else {
            geometry_tasks.reserve(gene_exon_paths.size());
            for (const auto& [gene_idx, exon_paths] : gene_exon_paths) {
                const auto body = gene_body_paths.find(gene_idx);
                geometry_tasks.push_back(
                    {gene_idx, &exon_paths,
                     body == gene_body_paths.end() ? nullptr : &body->second,
                     gene_idx});
            }
        }
        std::sort(geometry_tasks.begin(), geometry_tasks.end(),
                  [](const GeometryTask& a, const GeometryTask& b) {
                      return a.key < b.key;
                  });
        // One task per transcript went through the scheduler and the ordered-commit ring
        // individually, so a graph with over a million transcripts paid a mutex handshake and a
        // ring slot per unit. Every other parallel phase here batches; this one did not. Batch it
        // the same way. Blocks still commit in ordinal order, so the reduction below sees results
        // in task order exactly as before.
        constexpr size_t kMaxGeometryBlockSize = 32;
        constexpr size_t kGeometryBlocksPerWorker = 4;
        const size_t desired_geometry_blocks = geometry_tasks.empty()
            ? 0
            : std::min(geometry_tasks.size(),
                       options.threads > std::numeric_limits<size_t>::max() /
                                             kGeometryBlocksPerWorker
                           ? geometry_tasks.size()
                           : options.threads * kGeometryBlocksPerWorker);
        const size_t geometry_block_size = geometry_tasks.empty()
            ? 1
            : std::min(kMaxGeometryBlockSize,
                       1 + (geometry_tasks.size() - 1) / desired_geometry_blocks);
        const size_t geometry_worker_count =
            std::min(options.threads,
                     geometry_tasks.empty()
                         ? size_t{1}
                         : 1 + (geometry_tasks.size() - 1) / geometry_block_size);
        parallel_transform_ordered_blocks(
            geometry_tasks.size(), options.threads, geometry_block_size,
            [&](size_t block, size_t begin, size_t end) {
                static_cast<void>(block);
                std::vector<GeometryResult> block_results;
                block_results.reserve(end - begin);
                const std::vector<handlegraph::path_handle_t> no_body;
                for (size_t index = begin; index < end; ++index) {
                    const GeometryTask& task = geometry_tasks[index];
                    block_results.push_back(record_geometry(
                        *task.exon_paths,
                        task.body_paths == nullptr ? no_body : *task.body_paths,
                        task.legacy_gene_idx));
                }
                return block_results;
            },
            [&](size_t block, size_t begin, size_t end,
                std::vector<GeometryResult> block_results) {
                static_cast<void>(block);
                static_cast<void>(end);
                // Results commit by stable numeric target/gene ID. Expensive graph traversal is
                // lock-free; only this reduction mutates shared splice/counter structures.
                for (size_t offset = 0; offset < block_results.size(); ++offset) {
                    const GeometryTask& task = geometry_tasks[begin + offset];
                    GeometryResult& result = block_results[offset];
                    if (task.legacy_gene_idx.has_value()) {
                        transcript_spans[*task.legacy_gene_idx] = std::move(result.spans);
                    }
                    for (const auto& [edge, target_id] : result.splice_owners) {
                        splice_edges[edge].insert(target_id);
                    }
                    d63_splice_target_edges_evaluated += result.evaluated;
                    d63_splice_target_edges_owned += result.owned;
                    d63_splice_target_edges_fragment_only += result.fragment_only;
                    d63_splice_target_edges_adjacent_vetoed += result.adjacent_vetoed;
                }
            });
        const double splice_geometry_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - splice_geometry_started).count();
        std::cerr << "panCollapse: initialization: phase=splice_geometry workers="
                  << geometry_worker_count << " units=" << geometry_tasks.size()
                  << " owned_edges=" << d63_splice_target_edges_owned
                  << " seconds=" << splice_geometry_seconds << '\n';

        if (body_t2g.transcript_specific) {
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
        }

        node_ledger_of = [&](int64_t node_id) -> NodeLedgerLease {
            return node_ledger_cache.get_or_build(node_id, [&]() {
                if (!graph.has_node(node_id)) {
                    throw std::runtime_error(
                        "GAMP/graph node-id-space mismatch: node " + std::to_string(node_id) +
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
                graph_input.for_each_node_occurrence(node_id, [&](const pancollapse::GraphPathStep& step) {
                    const uint64_t path_handle =
                        handlegraph::as_integer(step.path);
                    const auto it = ledger_path_info.find(path_handle);
                    if (it == ledger_path_info.end()) {
                        return true;
                    }
                    const bool path_is_reverse = graph.get_is_reverse(step.handle);
                    // Every reference path crossing the node (exon transcript or gene body),
                    // un-collapsed, is a scoreable reference for tally_read_group_into.
                    nl.ref_paths.push_back(
                        {path_handle, &it->second.path_name(), path_is_reverse});
                    if (exact_ex50) {
                        NodeLedger::ExactStep exact_step{
                            path_handle, path_is_reverse};
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
                return nl;
            });
        };
        ledger_lookup = [&](int64_t node_id, const std::function<void(const std::string&, bool)>& emit) {
            const NodeLedgerLease node = node_ledger_of(node_id);
            for (const NodeLedger::ReferenceStep& step : node->ref_paths) {
                emit(*step.path_name, step.path_is_reverse);
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
        const std::string* locus_parent = nullptr;
        const std::string* path = nullptr;
        const std::string* parent = nullptr;
        uint32_t locus_parent_rank = 0;
        uint32_t path_rank = 0;
        uint32_t parent_rank = 0;
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
        return std::make_tuple(a.target_id, a.locus_parent_rank, a.direction,
                               tier_rank(a.tier), a.path_rank, a.parent_rank) <
               std::make_tuple(b.target_id, b.locus_parent_rank, b.direction,
                               tier_rank(b.tier), b.path_rank, b.parent_rank);
    };
    using ExactEx50EvidenceSet = std::vector<ExactEx50Evidence>;

    pancollapse::BoundedReferenceCache<int64_t, std::vector<size_t>>
        exact_model_candidates(
            std::numeric_limits<size_t>::max(),
            [](const std::vector<size_t>& candidates) {
                return sizeof(candidates) + candidates.capacity() * sizeof(size_t);
            });

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


    std::atomic<uint64_t> exact_unstarted_model_candidates_before_prefilter{0};
    std::atomic<uint64_t> exact_unstarted_model_candidates_after_prefilter{0};

    auto exact_ex50_evidence_for =
        [&](const vg::MultipathAlignment& alignment, const auto& read_node_ledger_of)
        -> ExactEx50EvidenceSet {
        ExactEx50EvidenceSet evidence;
        if (!exact_ex50 || alignment.subpath_size() == 0) return evidence;
        // D105: a touched node is evidence, not a promise that the entire read
        // lies on one ordered reference walk. Keep the full route denominator.
        struct State {
            size_t model_id = 0;
            uint64_t bases = 0, exonic = 0, forward = 0, reverse = 0;
            int64_t previous = 0, score = 0;
            bool previous_reverse = false, previous_end = false;
            bool concordant = true, body = false;
            auto key() const {
                return std::tie(model_id,bases,exonic,forward,reverse,previous,
                                previous_reverse,previous_end,concordant,body);
            }
        };
        auto dedup = [](std::vector<State>& states) {
            std::sort(states.begin(),states.end(),[](const auto& a,const auto& b){
                return a.key()!=b.key()?a.key()<b.key():a.score>b.score;
            });
            states.erase(std::unique(states.begin(),states.end(),[](const auto& a,const auto& b){
                return a.key()==b.key();
            }),states.end());
        };
        auto bases_of=[](const vg::Mapping& mapping){
            uint64_t n=0;for(const auto& edit:mapping.edit())n+=edit.from_length();return n;
        };
        std::vector<size_t> candidates;
        for(const auto& subpath:alignment.subpath())for(const auto& mapping:subpath.path().mapping()){
            if(!bases_of(mapping))continue;
            const auto& node=read_node_ledger_of(mapping.position().node_id());
            for(const auto* steps:{&node.exact_exon_steps,&node.exact_body_steps})
                for(const auto& step:*steps){
                    auto found=exact_ex50_models_by_body_path.find(step.path_handle);
                    if(found!=exact_ex50_models_by_body_path.end())
                        candidates.insert(candidates.end(),found->second.begin(),found->second.end());
                }
        }
        std::sort(candidates.begin(),candidates.end());
        candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
        const size_t count=alignment.subpath_size();
        std::vector<std::vector<State>> incoming(count);
        std::vector<bool> sources(count,alignment.start_size()==0);
        if(alignment.start_size())for(auto start:alignment.start()){
            if(start>=count)throw std::runtime_error("GAMP start index out of range");
            sources[start]=true;
        } else for(const auto& subpath:alignment.subpath()){
            for(auto next:subpath.next()){if(next>=count)throw std::runtime_error("GAMP next index out of range");sources[next]=false;}
            for(auto edge:subpath.connection()){if(edge.next()>=count)throw std::runtime_error("GAMP connection out of range");sources[edge.next()]=false;}
        }
        for(size_t s=0;s<count;++s)if(sources[s])
            for(auto model:candidates){State state;state.model_id=model;incoming[s].push_back(state);}
        for(size_t s=0;s<count;++s){
            auto states=std::move(incoming[s]);dedup(states);
            const auto& subpath=alignment.subpath(s);
            for(auto& state:states)state.score+=subpath.score();
            for(const auto& mapping:subpath.path().mapping()){
                const auto bases=bases_of(mapping);if(!bases)continue;
                const auto node_id=mapping.position().node_id();
                const auto& node=read_node_ledger_of(node_id);
                const auto offset=uint64_t(mapping.position().offset());
                if(offset>node.node_length||bases>node.node_length-offset)
                    throw std::runtime_error("GAMP mapping exceeds graph node");
                const bool reverse=mapping.position().is_reverse();
                for(auto& state:states){
                    const auto& model=exact_ex50_models[state.model_id];
                    const auto exon=exact_step_range(node.exact_exon_steps,model.exon_path_handle);
                    const auto body=exact_step_range(node.exact_body_steps,model.body_path_handle);
                    const bool exonic=exon.first!=exon.second;
                    state.bases+=bases;if(exonic)state.exonic+=bases;
                    state.body|=body.first!=body.second;
                    bool f=false,r=false;
                    const auto range=exonic?exon:body;
                    for(auto it=range.first;it!=range.second;++it){
                        const bool same=reverse==it->path_is_reverse;
                        f|=same;r|=!same;
                    }
                    // Ties are forward, as in the ordinary node-direction tally.
                    if(f)state.forward+=bases;else if(r)state.reverse+=bases;
                    if(state.previous!=0){
                        const ExactEdge edge{state.previous,state.previous_reverse,node_id,reverse};
                        const bool ordinary=std::binary_search(model.body_edges->begin(),model.body_edges->end(),edge);
                        if(!ordinary&&!(state.previous==node_id&&state.previous_reverse==reverse))
                            state.concordant&=state.previous_end&&offset==0&&
                                std::binary_search(model.exon_edges->begin(),model.exon_edges->end(),edge);
                    }
                    state.previous=node_id;state.previous_reverse=reverse;
                    state.previous_end=offset+bases==node.node_length;
                }
            }
            auto send=[&](uint32_t next,int64_t score){
                if(next>=count||next<=s)throw std::runtime_error("GAMP subpaths not topologically ordered");
                for(auto state:states){state.score+=score;incoming[next].push_back(std::move(state));}
            };
            for(auto next:subpath.next())send(next,0);
            for(auto connection:subpath.connection())send(connection.next(),connection.score());
            if(subpath.next_size()||subpath.connection_size())continue;
            for(const auto& state:states){
                if(!state.bases||(!state.exonic&&!state.body))continue;
                const auto& model=exact_ex50_models[state.model_id];
                const auto tier=pathtally::classify_ex50_tier(state.exonic,state.bases,state.concordant);
                const bool body_tier=tier==pathtally::Ex50Tier::Body;
                if(body_tier&&!state.body)continue;
                evidence.push_back({model.target_id,&model.exon->annotation.unique_parent.str(),
                    body_tier?&model.body->vg_path_name.str():&model.exon->vg_path_name.str(),
                    body_tier?&model.body->annotation.unique_parent.str():&model.exon->annotation.unique_parent.str(),
                    model.exon_parent_rank,body_tier?model.body_path_rank:model.exon_path_rank,
                    body_tier?model.body_parent_rank:model.exon_parent_rank,
                    state.forward>=state.reverse?'F':'R',tier,state.score});
            }
        }
        pancollapse::sort_best_evidence(evidence,exact_evidence_less);
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

    auto tally_numeric_ledger_group_into =
        [&](pathtally::NumericTallyMap& tallies,
            ReadNodeCache& read_nodes,
            const std::vector<const vg::MultipathAlignment*>& records) {
        tallies.clear();
        if (!pathtally::structural_tally_summed()) {
            // D091: each HST keeps its best alignment path. Node ledgers resolve once per read
            // through read_nodes, backed by the shared node ledger cache.
            thread_local pathtally::RouteTally<uint64_t> route_tally;
            route_tally.run(
                records, node_scorer,
                [&](int64_t node_id, const auto& emit) {
                    for (const auto& reference :
                         read_nodes.resolve(node_id, node_ledger_of).ref_paths) {
                        emit(reference.path_handle, reference.path_is_reverse);
                    }
                },
                [&](uint64_t path_handle, const pathtally::RouteScore& best) {
                    tallies[path_handle] = {best.score, best.forward_bases, best.reverse_bases};
                });
            return;
        }
        std::vector<int64_t> per_node;
        auto visit_mappings = [&](const auto& visit) {
            for (const vg::MultipathAlignment* record : records) {
                const vg::MultipathAlignment& alignment = *record;
                const std::vector<size_t> offsets = pathtally::subpath_read_offsets(alignment);
                for (int subpath_index = 0; subpath_index < alignment.subpath_size();
                     ++subpath_index) {
                    const vg::Subpath& subpath = alignment.subpath(subpath_index);
                    node_scorer(alignment, subpath_index,
                                offsets[static_cast<size_t>(subpath_index)], per_node);
                    const vg::Path& path = subpath.path();
                    for (int mapping_index = 0; mapping_index < path.mapping_size();
                         ++mapping_index) {
                        const vg::Mapping& mapping = path.mapping(mapping_index);
                        const bool read_is_reverse = mapping.position().is_reverse();
                        const int64_t node_score = per_node[static_cast<size_t>(mapping_index)];
                        const int64_t aligned_bases = pathtally::mapping_aligned_bases(mapping);
                        visit(mapping.position().node_id(), read_is_reverse, node_score,
                              aligned_bases);
                    }
                }
            }
        };
        bool fits = true;
        visit_mappings([&](int64_t node_id, bool reverse, int64_t score, int64_t bases) {
            fits &= read_nodes.add(node_id, reverse, score, bases);
        });
        if (fits && read_nodes.expand(tallies, node_ledger_of)) return;

        // Preserve the old mapping-order arithmetic on a regrouping overflow.
        tallies.clear();
        visit_mappings([&](int64_t node_id, bool reverse, int64_t score, int64_t bases) {
            for (const auto& reference : read_nodes.resolve(node_id, node_ledger_of).ref_paths) {
                pathtally::HstTally& tally = tallies[reference.path_handle];
                tally.score += score;
                if (reverse == reference.path_is_reverse) tally.forward_bases += bases;
                else tally.reverse_bases += bases;
            }
        });
    };

    std::filesystem::create_directories(options.out_dir);
    uint64_t max_chunk_bytes = static_cast<uint64_t>(1) << 30;  // 1 GiB, well under the u32 chunk field
    if (const char* env = std::getenv("PANCOLLAPSE_MAX_CHUNK_BYTES")) {
        max_chunk_bytes = std::strtoull(env, nullptr, 10);
        if (max_chunk_bytes == 0) {
            max_chunk_bytes = 1;
        }
    }
    auto rad_writer = std::make_unique<RadStreamWriter>(
        options.out_dir / "map.rad", t2g.target_names, options.raw_cb_length,
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

    auto start_group = [&](std::string_view name) {
        std::string group_name(name);
        if (completed_names.count(group_name) != 0) {
            ++grouping_recurrence_failures;
            throw std::runtime_error("grouping_recurrence_failures: completed GAMP read name recurred");
        }
        current_group = {};
        current_group.name = std::move(group_name);
        ++input_read_groups;
        const MoleculeParseResult molecule =
            parse_molecule_id(current_group.name, options.raw_cb_length,
                              options.raw_umi_length);
        if (molecule.status == MoleculeParseStatus::Ok) {
            current_group.molecule = molecule.id;
        } else {
            current_group.skip_for_molecule_identity = true;
            current_group.molecule_status = molecule.status;
            current_group.molecule_message = molecule.message;
            current_group.molecule.original_name = molecule.id.original_name;
            if (options.molecule_identity_failures == MoleculeIdentityFailurePolicy::Fail) {
                throw std::runtime_error(molecule_status_counter(molecule.status) + "=1: " + molecule.message);
            }
        }
        have_group = true;
    };

    const auto processing_started = std::chrono::steady_clock::now();
    // A thread handoff plus deterministic output turn costs more than evaluating a tiny exact
    // surface. Keep toy/small-reference conversions serial even when a larger ceiling was
    // requested; chromosome/pangenome ledgers exceed this threshold by orders of magnitude.
    constexpr size_t kMinExactModelsForWorkerPool = 32;
    const size_t requested_processing_threads =
        exact_ex50 && exact_ex50_models.size() < kMinExactModelsForWorkerPool
            ? 1
            : options.threads;
    if (requested_processing_threads != options.threads) {
        std::cerr << "panCollapse: processing workers: requested=" << options.threads
                  << " active=" << requested_processing_threads
                  << " reason=small_exact_model_surface exact_models="
                  << exact_ex50_models.size() << '\n';
    }
    const size_t processing_threads = requested_processing_threads;
    const bool profile_runtime_timing =
        std::getenv("PANCOLLAPSE_PROFILE_TIMING") != nullptr;
    struct alignas(64) WorkerRuntimeTiming {
        uint64_t groups = 0;
        uint64_t queue_wait_ns = 0;
        uint64_t compute_before_output_ns = 0;
        uint64_t ordered_wait_ns = 0;
        uint64_t ordered_region_ns = 0;
        uint64_t prelude_ns = 0;
        uint64_t tally_ns = 0;
        uint64_t evidence_ns = 0;
        uint64_t tail_ns = 0;
    };
    std::vector<WorkerRuntimeTiming> worker_runtime_timing(processing_threads);
    uint64_t producer_admission_wait_ns = 0;
    size_t queue_high_water = 0;
    auto producer_finished = processing_started;
    auto next_progress_report = processing_started + std::chrono::minutes(5);
    size_t next_progress_group = 1000000;
    std::atomic<size_t> completed_groups_for_progress{0};
    OrderedOutputCoordinator ordered_output(processing_threads > 1);

    auto process_group = [&](Group group, size_t ordinal,
                             pathtally::TallyMap& tally_workspace,
                             pathtally::NumericTallyMap& numeric_tally_workspace,
                             ReadNodeCache& read_nodes,
                             WorkerRuntimeTiming& runtime_timing) {
        Group& current_group = group;
        read_nodes.clear();
        const auto read_node_ledger_of = [&](int64_t node_id) -> const NodeLedger& {
            return read_nodes.resolve(node_id, node_ledger_of);
        };
        const size_t current_read_group = ordinal + 1;
        OrderedOutputCoordinator::Guard output_guard(ordered_output, ordinal);
        const auto group_started = profile_runtime_timing
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
        bool output_phase_started = false;
        std::chrono::steady_clock::time_point output_wait_started_at;
        std::chrono::steady_clock::time_point output_phase_started_at;
        // Region clock for the read path. Regions nest inside compute_before_output_ns, so
        // their sum plus tail is a check on the instrumentation itself: a shortfall means
        // real work sits outside every bracket.
        auto region_clock = group_started;
        auto mark = [&](uint64_t& bucket) {
            if (!profile_runtime_timing) return;
            // A bracket can straddle acquire_output (notably observe). Clip it
            // to the end of compute, excluding both ordered wait and output.
            const auto now = output_phase_started
                ? output_wait_started_at : std::chrono::steady_clock::now();
            bucket += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - region_clock)
                    .count());
            region_clock = now;
        };
        auto acquire_output = [&]() {
            if (!profile_runtime_timing || output_phase_started) {
                output_guard.acquire();
                return;
            }
            const auto wait_started = std::chrono::steady_clock::now();
            output_wait_started_at = wait_started;
            runtime_timing.compute_before_output_ns += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    wait_started - group_started).count());
            output_guard.acquire();
            output_phase_started_at = std::chrono::steady_clock::now();
            runtime_timing.ordered_wait_ns += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    output_phase_started_at - wait_started).count());
            output_phase_started = true;
        };
        auto emit_ordered = [&](std::function<void()> action) {
            acquire_output();
            action();
        };
        auto process = [&]() {
        if (current_group.skip_for_molecule_identity) {
            acquire_output();
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
            emit_ordered([warning = "panCollapse: warning: skipped read group with invalid raw "
                                    "molecule identity: " + current_group.name + ": " +
                                    current_group.molecule_message + '\n'] {
                std::cerr << warning;
            });
            return;
        }

        bool bam_record_written = false;
        auto write_barcode_only = [&]() {
            if (!bam_writer || bam_record_written) {
                return;
            }
            acquire_output();
            const std::string empty;
            const vg::MultipathAlignment* read =
                current_group.records.empty() ? nullptr : current_group.records.front();
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
            for (const vg::MultipathAlignment* record_pointer : current_group.records) {
                const vg::MultipathAlignment& record = *record_pointer;
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
            for (const vg::MultipathAlignment* record_pointer : current_group.records) {
                const vg::MultipathAlignment& record = *record_pointer;
                record_ptrs.push_back(&record);
            }
            mark(runtime_timing.prelude_ns);
            if (production_identity) {
                tally_numeric_ledger_group_into(numeric_tally_workspace, read_nodes, record_ptrs);
            } else {
                pathtally::tally_read_group_into(
                    tally_workspace, record_ptrs, ledger_lookup, node_scorer);
            }
            mark(runtime_timing.tally_ns);

            // Score-independent aligned-base orientation and touched body ranges are retained for
            // the legacy two-column mode. D063 orientation is derived later from the winning raw
            // paths for the exact transcript/layer that supplied its S/U call, so it skips this
            // mapping traversal entirely.
            std::optional<std::map<uint32_t, std::pair<int64_t, int64_t>>> gene_orient;
            std::optional<std::map<uint32_t, std::pair<int64_t, int64_t>>> body_range;
            if (!body_t2g.transcript_specific) {
                gene_orient.emplace();
                body_range.emplace();
                for (const vg::MultipathAlignment* record_pointer : current_group.records) {
                    const vg::MultipathAlignment& record = *record_pointer;
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
                            const NodeLedger& nl = read_node_ledger_of(node_id);
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
            std::map<uint32_t, pathtally::CompactCollapsedIdentityTally>
                exon_identity_evidence;
            std::map<uint32_t, pathtally::CompactCollapsedIdentityTally>
                body_identity_evidence;
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
                pathtally::CompactIdentityLayers layers =
                    pathtally::collapse_ranked_identity_tallies(
                        numeric_tally_workspace,
                        [&](uint64_t path_handle)
                            -> std::optional<pathtally::RankedPathIdentity> {
                            const auto found = ledger_path_info.find(path_handle);
                            if (found == ledger_path_info.end()) {
                                throw std::runtime_error(
                                    "internal numeric path tally is absent from the ledger catalog");
                            }
                            const PathInfo& path_info = found->second;
                            if (!path_info.is_exon && !path_info.body_resolvable) {
                                return std::nullopt;
                            }
                            return pathtally::RankedPathIdentity{
                                &path_info.path_name(),
                                &path_info.identity->annotation.unique_parent.str(),
                                path_info.parent_rank, path_info.target_id,
                                path_info.is_exon};
                        });
                exon_identity_evidence = std::move(layers.exon);
                body_identity_evidence = std::move(layers.body);
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

            ExactEx50EvidenceSet exact_group_evidence;
            if (exact_ex50) {
                for (const vg::MultipathAlignment* record_pointer : current_group.records) {
                    const vg::MultipathAlignment& record = *record_pointer;
                    ExactEx50EvidenceSet record_evidence =
                        exact_ex50_evidence_for(record, read_node_ledger_of);
                    exact_group_evidence.insert(exact_group_evidence.end(),
                                                record_evidence.begin(), record_evidence.end());
                }
                pancollapse::sort_best_evidence(exact_group_evidence, exact_evidence_less);
                // Reference filtering changes the compatible transcript surface, so it must run
                // before the top-score-minus-5 window and before the global E/P/B rank. Filtering
                // a projected winner downstream cannot recover an allowed lower candidate.
                if (!strict_allowlisted_parents.empty()) {
                    const size_t before = exact_group_evidence.size();
                    std::erase_if(
                        exact_group_evidence,
                        [&](const ExactEx50Evidence& evidence) {
                            return strict_allowlisted_parents.count(*evidence.parent) == 0;
                        });
                    strict_allowlisted_evidence_dropped.fetch_add(
                        before - exact_group_evidence.size(), std::memory_order_relaxed);
                }
                if (options.exact_ex50_score_window_enabled && !exact_group_evidence.empty()) {
                    int64_t top_score = std::numeric_limits<int64_t>::min();
                    for (const ExactEx50Evidence& evidence : exact_group_evidence) {
                        top_score = std::max(top_score, evidence.score);
                    }
                    std::erase_if(exact_group_evidence, [&](const ExactEx50Evidence& evidence) {
                        return evidence.score < top_score - kExactEx50ScoreWindow;
                    });
                }
            }

            // Debug rows are ordered output, but exact Ex50 DP is read-local work. Delay taking
            // the output turn until after that DP so requesting diagnostics does not silently
            // serialize the dominant production computation.
            if (debug_writer) {
                acquire_output();
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
                            current_group.molecule.original_name, layer,
                            production_identity ? identity_ledger->original_transcript(transcript) : transcript,
                            t2g.transcript_gene.at(transcript), score,
                            is_concordant(transcript_id));
                    }
                };
                write_exact_top("exon", exon_score);
                write_exact_top("body", body_score);
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
            // pathtally builds `calls` by draining a std::map keyed on the transcript target
            // id, so the ids are unique and ascending and a call's own position is a complete
            // key. One char per call replaces a std::map whose every node was a separate
            // allocation, on every read.
            std::vector<char> transcript_gd;  // parallel to calls: 'F'/'R'
            transcript_gd.reserve(calls.size());
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
                transcript_gd.push_back(forward ? 'F' : 'R');
            }
            const auto call_strand = [&](const pathtally::LedgerCall& call) {
                return transcript_gd[static_cast<size_t>(&call - calls.data())];
            };

            // The production normal BAM is a versioned typed union. G rows retain ordinary S/U
            // evidence and carry '.' in exact fields; E rows retain exact Parent evidence and carry
            // '.' in GL. Repeated TX values are evidence rows, not a transcript->gene ambiguity:
            // TX->GX remains single-valued and only identical complete rows deduplicate.
            if (bam_writer) {
                acquire_output();
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
                    xu += *evidence.parent;
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
                    gd += call_strand(call);
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
                    xp += *evidence.path;
                    xu += *evidence.parent;
                    bam_genes.insert(gene);
                    if (primary_gene.empty()) {
                        primary_gene = gene;
                        primary_tid = t2g.gene_ids.at(gene);
                    }
                }
                const bool multigene = bam_genes.size() > 1;
                const bool has_xt = !multigene || options.bam_multigene == BamMultiGenePolicy::First;
                const vg::MultipathAlignment& read =
                    *current_group.records.front();
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
                    gd += call_strand(call);
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
                    const vg::MultipathAlignment& read = *current_group.records.front();
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
                                   call_strand(call) == 'F', {}, {}});
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
            emit_ordered([writer = rad_writer.get(), molecule = current_group.molecule,
                          hits]() { writer->write_record(molecule, hits); });
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
            const vg::MultipathAlignment& read = *current_group.records.front();
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
            emit_ordered([&histogram = emitted_target_histogram, size = hits.size()] {
                ++histogram[size];
            });
        }
        write_barcode_only();
        };
        process();
        mark(runtime_timing.tail_ns);
        // Retire every ordinal, including a group that emitted nothing.
        acquire_output();
        const size_t completed =
            completed_groups_for_progress.fetch_add(1, std::memory_order_relaxed) + 1;
        auto report_progress = [&]() {
            const bool group_report = completed >= next_progress_group;
            const auto progress_now = std::chrono::steady_clock::now();
            if (group_report || progress_now >= next_progress_report) {
                const double elapsed = std::chrono::duration<double>(
                    progress_now - processing_started).count();
                std::cerr << "panCollapse: progress: completed_read_groups="
                          << completed << " elapsed_seconds=" << elapsed
                          << " groups_per_second="
                          << (elapsed > 0.0 ? completed / elapsed : 0.0) << '\n';
                next_progress_report = progress_now + std::chrono::minutes(5);
                next_progress_group = completed + 1000000;
            }
        };
        if (completed >= next_progress_group || completed % 4096 == 0) {
            report_progress();
        }
        if (profile_runtime_timing) {
            if (output_phase_started) {
                runtime_timing.ordered_region_ns += static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - output_phase_started_at).count());
            } else {
                runtime_timing.compute_before_output_ns += static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - group_started).count());
            }
            ++runtime_timing.groups;
        }
    };

    // Keep at most two queued groups per worker. Active groups add at most one more per worker,
    // so scheduling memory is O(threads) and independent of the total GAMP length.
    const size_t max_queued_groups =
        processing_threads > std::numeric_limits<size_t>::max() / 2
            ? std::numeric_limits<size_t>::max()
            : std::max<size_t>(1, processing_threads * 2);
    std::mutex jobs_mutex;
    std::condition_variable jobs_ready;
    std::condition_variable jobs_have_space;
    std::deque<GroupJob> jobs;
    bool jobs_closed = false;
    bool stop_workers = false;
    std::vector<std::thread> workers;
    pathtally::TallyMap serial_tally_workspace;
    pathtally::NumericTallyMap serial_numeric_tally_workspace;
    ReadNodeCache serial_read_nodes;

    auto worker_loop = [&](size_t worker_id) {
#ifdef __linux__
        pthread_setname_np(pthread_self(), "pc-convert");
#endif
        pathtally::TallyMap tally_workspace;
        pathtally::NumericTallyMap numeric_tally_workspace;
        ReadNodeCache read_nodes;
        WorkerRuntimeTiming& runtime_timing = worker_runtime_timing[worker_id];
        try {
            while (true) {
                GroupJob job;
                {
                    std::unique_lock<std::mutex> lock(jobs_mutex);
                    if (!stop_workers && jobs.empty() && !jobs_closed) {
                        jobs_ready.wait(lock, [&] {
                            return stop_workers || !jobs.empty() || jobs_closed;
                        });
                    }
                    if (stop_workers || (jobs.empty() && jobs_closed)) {
                        break;
                    }
                    job = std::move(jobs.front());
                    jobs.pop_front();
                }
                if (profile_runtime_timing) {
                    runtime_timing.queue_wait_ns += static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - job.submitted_at).count());
                }
                jobs_have_space.notify_one();
                process_group(std::move(job.group), job.ordinal, tally_workspace,
                              numeric_tally_workspace, read_nodes, runtime_timing);
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

    if (processing_threads > 1) {
        workers.reserve(processing_threads);
        try {
            for (size_t i = 0; i < processing_threads; ++i) {
                workers.emplace_back(worker_loop, i);
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
        // Moving the group moves owned_records' buffer, so these stay valid.
        for (const vg::MultipathAlignment& record : current_group.owned_records) {
            current_group.records.push_back(&record);
        }
        GroupJob job{ordinal, std::move(current_group), {}};
        have_group = false;
        if (processing_threads == 1) {
            process_group(std::move(job.group), job.ordinal, serial_tally_workspace,
                          serial_numeric_tally_workspace, serial_read_nodes,
                          worker_runtime_timing.front());
            return;
        }
        std::unique_lock<std::mutex> lock(jobs_mutex);
        const auto admission_wait_started = std::chrono::steady_clock::now();
        jobs_have_space.wait(lock, [&] {
            return stop_workers || jobs.size() < max_queued_groups;
        });
        producer_admission_wait_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - admission_wait_started).count());
        if (stop_workers) {
            lock.unlock();
            const std::exception_ptr failure = ordered_output.failure();
            if (failure) {
                std::rethrow_exception(failure);
            }
            throw std::runtime_error("PanCollapse worker pool stopped without an exception");
        }
        if (profile_runtime_timing) {
            job.submitted_at = std::chrono::steady_clock::now();
        }
        jobs.push_back(std::move(job));
        if (profile_runtime_timing) {
            queue_high_water = std::max(queue_high_water, jobs.size());
        }
        lock.unlock();
        jobs_ready.notify_one();
    };

    std::exception_ptr producer_failure;
    try {
        const auto consume_alignment = [&](vg::MultipathAlignment& alignment) {
            ++input_records;
            if (!have_group) {
                start_group(alignment.name());
            } else if (alignment.name() != current_group.name) {
                submit_group();
                start_group(alignment.name());
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
                    current_group.owned_records.push_back(std::move(alignment));
                }
            }
        };
        vg::io::for_each<vg::MultipathAlignment>(*gamp_in, consume_alignment);
        submit_group();
        producer_finished = std::chrono::steady_clock::now();
    } catch (...) {
        producer_finished = std::chrono::steady_clock::now();
        producer_failure = std::current_exception();
        ordered_output.fail(producer_failure);
        std::lock_guard<std::mutex> lock(jobs_mutex);
        stop_workers = true;
    }
    if (processing_threads > 1) {
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

    if (profile_runtime_timing) {
        WorkerRuntimeTiming sum;
        WorkerRuntimeTiming maximum;
        for (const WorkerRuntimeTiming& timing : worker_runtime_timing) {
            sum.groups += timing.groups;
            sum.queue_wait_ns += timing.queue_wait_ns;
            sum.compute_before_output_ns += timing.compute_before_output_ns;
            sum.ordered_wait_ns += timing.ordered_wait_ns;
            sum.ordered_region_ns += timing.ordered_region_ns;
            sum.prelude_ns += timing.prelude_ns;
            sum.tally_ns += timing.tally_ns;
            sum.evidence_ns += timing.evidence_ns;
            sum.tail_ns += timing.tail_ns;
            maximum.queue_wait_ns = std::max(maximum.queue_wait_ns, timing.queue_wait_ns);
            maximum.compute_before_output_ns = std::max(
                maximum.compute_before_output_ns, timing.compute_before_output_ns);
            maximum.ordered_wait_ns = std::max(
                maximum.ordered_wait_ns, timing.ordered_wait_ns);
            maximum.ordered_region_ns = std::max(
                maximum.ordered_region_ns, timing.ordered_region_ns);
        }
        constexpr double kNanosecondsPerSecond = 1e9;
        const double producer_wall_seconds = std::chrono::duration<double>(
            producer_finished - processing_started).count();
        const double producer_admission_wait_seconds =
            producer_admission_wait_ns / kNanosecondsPerSecond;
        const uint64_t region_sum_ns =
            sum.prelude_ns + sum.tally_ns + sum.evidence_ns + sum.tail_ns;
        std::cerr << "panCollapse: read-path regions:"
                  << " prelude_seconds=" << sum.prelude_ns / kNanosecondsPerSecond
                  << " tally_seconds=" << sum.tally_ns / kNanosecondsPerSecond
                  << " evidence_seconds=" << sum.evidence_ns / kNanosecondsPerSecond
                  << " tail_seconds=" << sum.tail_ns / kNanosecondsPerSecond
                  << " region_sum_seconds=" << region_sum_ns / kNanosecondsPerSecond
                  << " compute_total_seconds="
                  << sum.compute_before_output_ns / kNanosecondsPerSecond
                  << " (worker-seconds; region_sum should approach compute_total)\n";
        std::cerr << "panCollapse: runtime profile: groups=" << sum.groups
                  << " queue_high_water=" << queue_high_water
                  << " producer_wall_seconds=" << producer_wall_seconds
                  << " producer_admission_wait_seconds="
                  << producer_admission_wait_seconds
                  << " producer_active_seconds="
                  << std::max(0.0, producer_wall_seconds -
                                       producer_admission_wait_seconds)
                  << " worker_queue_wait_sum_seconds="
                  << sum.queue_wait_ns / kNanosecondsPerSecond
                  << " worker_queue_wait_max_seconds="
                  << maximum.queue_wait_ns / kNanosecondsPerSecond
                  << " worker_compute_before_output_sum_seconds="
                  << sum.compute_before_output_ns / kNanosecondsPerSecond
                  << " worker_compute_before_output_max_seconds="
                  << maximum.compute_before_output_ns / kNanosecondsPerSecond
                  << " worker_ordered_wait_sum_seconds="
                  << sum.ordered_wait_ns / kNanosecondsPerSecond
                  << " worker_ordered_wait_max_seconds="
                  << maximum.ordered_wait_ns / kNanosecondsPerSecond
                  << " worker_ordered_region_sum_seconds="
                  << sum.ordered_region_ns / kNanosecondsPerSecond
                  << " worker_ordered_region_max_seconds="
                  << maximum.ordered_region_ns / kNanosecondsPerSecond
                  << '\n';
    }

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
    std::cerr << "panCollapse: performance: workers=" << processing_threads
              << " requested_workers=" << options.threads
              << " initialization_seconds=" << initialization_seconds
              << " processing_seconds=" << processing_seconds
              << " input_records=" << input_records
              << " input_read_groups=" << input_read_groups
              << " groups_per_second="
              << (processing_seconds > 0.0 ? input_read_groups / processing_seconds : 0.0)
              << " node_cache_entries="
              << (options.count_mode == pathtally::CountMode::Score
                      ? node_hst_cache.build_count()
                      : node_ledger_cache.build_count())
              << " exact_exon_edge_geometries=" << exact_exon_edge_geometry_count
              << " exact_model_candidate_cache_entries="
              << exact_model_candidates.build_count()
              << '\n';

    rad_writer->finalize();
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
                        std::to_string(rad_writer->record_count()) + "\nno_compatible_transcript_groups\t" +
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
        if (first_arg == "index") return run_annotation_index(argc, argv);
        if (first_arg == "count") return run_native_count(argc, argv);
        if (first_arg == "convert") {
            std::cerr << "panCollapse: warning: 'convert' is deprecated; "
                         "use 'index' and 'count' for native gene counting.\n";
        }
        return run_convert(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "panCollapse: " << e.what() << '\n';
        return 1;
    }
}
