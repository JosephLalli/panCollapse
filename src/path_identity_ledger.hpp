#pragma once

#include "compact_identity_text.hpp"
#include "frozen_flat_map.hpp"

// Strict reader for the production panSC post-vg path identity ledger.  This is
// intentionally separate from the historical 2/3-column t2g adapter: callers
// select the schema explicitly and no file-shape auto-detection occurs.

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace path_identity {

inline constexpr const char* kSchemaVersion = "panSC-path-identity-v1";

// Columns the runtime never reads. They back a body/exon consistency check and the
// fallback validation below, both of which still run whenever the columns are present, but a
// ledger that omits them is accepted: nothing in counting, and nothing in the compatibility
// structural surface digest, consults them. Keeping them optional lets a producer emit a
// ledger carrying only what panCollapse actually needs.
inline constexpr std::array<const char*, 9> kOptionalColumns = {
    "vg_haplotype_origins", "source_parent",     "input_parent",
    "haplotype",            "annotation_source", "selection_status",
    "fallback_status",      "source_transcript", "transcript_class",
};

inline bool is_optional_column(std::string_view name) {
    for (const char* optional : kOptionalColumns) {
        if (name == optional) return true;
    }
    return false;
}

// The canonical column order. Every column a ledger does carry must appear in this order.
inline constexpr std::array<const char*, 24> kRequiredColumns = {
    "schema_version",
    "vg_path_name",
    "vg_path_length",
    "vg_haplotype_origins",
    "unique_parent",
    "source_parent",
    "input_parent",
    "canonical_transcript",
    "gene_id",
    "source_path_or_contig",
    "sample",
    "haplotype",
    "annotation_source",
    "feature_layer",
    "selection_status",
    "fallback_status",
    "exon_unique_parent",
    "gene_locus",
    "source_gene",
    "source_transcript",
    "transcript_class",
    "strand",
    "start",
    "end",
};

// Needed only while validating rows and linked exon/body provenance. Production
// evidence, counting and structural hashes never read these audit-only columns.
struct AnnotationAudit {
    pancollapse::CompactIdentityText source_parent;
    pancollapse::CompactIdentityText input_parent;
    pancollapse::CompactIdentityText haplotype;
    pancollapse::CompactIdentityText annotation_source;
    pancollapse::CompactIdentityText selection_status;
    pancollapse::CompactIdentityText fallback_status;
    pancollapse::CompactIdentityText source_transcript;
    pancollapse::CompactIdentityText transcript_class;

    bool operator==(const AnnotationAudit&) const = default;
};

struct AnnotationIdentity {
    pancollapse::CompactIdentityText unique_parent;
    pancollapse::CompactIdentityText canonical_transcript;
    pancollapse::CompactIdentityText gene_id;
    pancollapse::CompactIdentityText source_path_or_contig;
    pancollapse::CompactIdentityText sample;
    pancollapse::CompactIdentityText feature_layer;
    pancollapse::CompactIdentityText exon_unique_parent;
    pancollapse::CompactIdentityText gene_locus;
    pancollapse::CompactIdentityText source_gene;
    pancollapse::CompactIdentityText strand;
    pancollapse::CompactIdentityText start;
    pancollapse::CompactIdentityText end;
    std::unique_ptr<const AnnotationAudit> audit;

    bool operator==(const AnnotationIdentity& other) const {
        return unique_parent == other.unique_parent &&
            canonical_transcript == other.canonical_transcript && gene_id == other.gene_id &&
            source_path_or_contig == other.source_path_or_contig && sample == other.sample &&
            feature_layer == other.feature_layer && exon_unique_parent == other.exon_unique_parent &&
            gene_locus == other.gene_locus && source_gene == other.source_gene &&
            strand == other.strand && start == other.start && end == other.end &&
            ((!audit && !other.audit) || (audit && other.audit && *audit == *other.audit));
    }
};

struct PathIdentityRow {
    AnnotationIdentity annotation;
    pancollapse::CompactIdentityText vg_path_name;
    uint64_t vg_path_length = 0;
    // Numeric runtime binding; never included in identity/structural digests.
    uint64_t graph_handle = 0;
    uint32_t lexical_rank = 0;
    bool graph_bound = false;
};

inline constexpr std::string_view kQualifiedTargetPrefix = "panSCtxg1:";

struct QualifiedTranscriptTarget {
    pancollapse::CompactIdentityText target;
    pancollapse::CompactIdentityText source_gene;
    pancollapse::CompactIdentityText gene_id;
};

struct PathIdentityLedger {
    pancollapse::FrozenFlatMap<pancollapse::CompactIdentityText, PathIdentityRow> rows_by_path;
    // Trees validate arbitrary input order, then compact into lexical arrays.
    // Parent references are rebound once at compaction and survive ledger moves.
    pancollapse::FrozenFlatMap<pancollapse::CompactIdentityText, const AnnotationIdentity*>
        identities_by_parent;
    std::map<std::string, std::string> canonical_gene;
    // Count-only splits. Original row labels stay intact; no per-row target string
    // or second full transcript catalog is retained.
    std::map<pancollapse::CompactIdentityText,
             std::map<pancollapse::CompactIdentityText, QualifiedTranscriptTarget, std::less<>>,
             std::less<>> qualified_targets;
    std::unordered_map<pancollapse::CompactIdentityText, pancollapse::CompactIdentityText>
        normalized_source_genes;
    std::map<pancollapse::CompactIdentityText, pancollapse::CompactIdentityText, std::less<>>
        escaped_targets, original_transcripts;
    // Where these rows were read from, so a consumer deriving an identity from the ledger
    // does not have to be handed the path a second time alongside the ledger itself.
    std::filesystem::path source_path;
    bool has_body_layer = false;

    PathIdentityLedger() = default;
    PathIdentityLedger(const PathIdentityLedger&) = delete;
    PathIdentityLedger& operator=(const PathIdentityLedger&) = delete;
    PathIdentityLedger(PathIdentityLedger&&) = default;
    PathIdentityLedger& operator=(PathIdentityLedger&&) = default;

    const std::string& target_transcript(const AnnotationIdentity& identity) const {
        const auto qualified = qualified_targets.find(identity.canonical_transcript);
        if (qualified != qualified_targets.end()) {
            // Body/exon source and canonical labels have already been checked equal.
            const auto& source = normalized_source_genes.at(identity.source_gene);
            return qualified->second.at(source).target.str();
        }
        const auto escaped = escaped_targets.find(identity.canonical_transcript);
        return escaped == escaped_targets.end() ? identity.canonical_transcript.str()
                                               : escaped->second.str();
    }

    const std::string& original_transcript(const std::string& target) const {
        const auto found = original_transcripts.find(target);
        return found == original_transcripts.end() ? target : found->second.str();
    }

    size_t qualified_target_count() const {
        size_t result = 0;
        for (const auto& [canonical, targets] : qualified_targets) result += targets.size();
        return result;
    }
};

inline std::vector<std::string> split_tab(const std::string& line) {
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

inline void require_tsv_safe(const std::string& value, const std::string& field,
                             size_t line_number) {
    if (value.find_first_of("\t\r\n") != std::string::npos) {
        throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                 " field " + field + " contains a TSV line delimiter");
    }
}

inline void require_group_component_safe(const std::string& value, const std::string& field,
                                         size_t line_number, bool comma_is_reserved) {
    const bool unsafe = value.find(';') != std::string::npos ||
                        (comma_is_reserved && value.find(',') != std::string::npos);
    if (unsafe) {
        throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                 " field " + field +
                                 " contains a reserved BAM provenance delimiter");
    }
}

inline void require_not_bam_missing_sentinel(const std::string& value, const std::string& field,
                                             size_t line_number) {
    if (value == ".") {
        throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                 " field " + field +
                                 " collides with the BAM typed-union missing sentinel '.'");
    }
}

inline uint64_t parse_positive_length(const std::string& value, size_t line_number) {
    if (value.empty()) {
        throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                 " has empty vg_path_length");
    }
    uint64_t result = 0;
    for (const char c : value) {
        if (c < '0' || c > '9') {
            throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                     " has non-numeric vg_path_length");
        }
        const uint64_t digit = static_cast<uint64_t>(c - '0');
        if (result > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                     " has overflowing vg_path_length");
        }
        result = result * 10 + digit;
    }
    if (result == 0) {
        throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                 " has zero vg_path_length");
    }
    return result;
}

inline uint64_t parse_positive_coordinate(const std::string& value, const std::string& field,
                                          size_t line_number) {
    if (value.empty()) {
        throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                 " has empty " + field);
    }
    uint64_t result = 0;
    for (const char c : value) {
        if (c < '0' || c > '9') {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) + " has non-numeric " + field);
        }
        const uint64_t digit = static_cast<uint64_t>(c - '0');
        if (result > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) + " has overflowing " + field);
        }
        result = result * 10 + digit;
    }
    if (result == 0) {
        throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                 " has zero " + field);
    }
    return result;
}

inline PathIdentityLedger read(const std::filesystem::path& filename,
                               bool defer_linked_body_gene_mismatch = false,
                               const std::function<void(size_t, size_t, uint64_t)>& progress = {},
                               const std::function<std::string(const AnnotationIdentity&)>&
                                   source_gene_identity = {},
                               std::istream* supplied_stream = nullptr) {
    std::ifstream file;
    if (!supplied_stream) file.open(filename);
    std::istream& in = supplied_stream ? *supplied_stream : file;
    if (!in) {
        throw std::runtime_error("cannot open path identity ledger " + filename.string());
    }

    std::string header_line;
    if (!std::getline(in, header_line) || header_line.empty()) {
        throw std::runtime_error("path identity ledger is missing its header");
    }
    const std::vector<std::string> header = split_tab(header_line);
    // Accept a ledger that omits the optional columns. The columns present must still be a
    // subsequence of the canonical order, so a shuffled or unknown header is still refused.
    {
        size_t canonical = 0;
        for (size_t i = 0; i < header.size(); ++i) {
            while (canonical < kRequiredColumns.size() &&
                   header[i] != kRequiredColumns[canonical]) {
                if (!is_optional_column(kRequiredColumns[canonical])) {
                    throw std::runtime_error("path identity ledger is missing required column " +
                                             std::string(kRequiredColumns[canonical]));
                }
                ++canonical;
            }
            if (canonical == kRequiredColumns.size()) {
                throw std::runtime_error("path identity ledger header column " +
                                         std::to_string(i + 1) + " is " + header[i] +
                                         "; not a column of schema " +
                                         std::string(kSchemaVersion) +
                                         " or out of canonical order");
            }
            ++canonical;
        }
        for (; canonical < kRequiredColumns.size(); ++canonical) {
            if (!is_optional_column(kRequiredColumns[canonical])) {
                throw std::runtime_error("path identity ledger is missing required column " +
                                         std::string(kRequiredColumns[canonical]));
            }
        }
    }
    std::map<std::string, size_t> column;
    for (size_t i = 0; i < header.size(); ++i) {
        if (header[i].empty()) {
            throw std::runtime_error("path identity ledger header has an empty column name");
        }
        require_tsv_safe(header[i], "header", 1);
        if (!column.emplace(header[i], i).second) {
            throw std::runtime_error("path identity ledger header repeats column " + header[i]);
        }
    }
    for (const char* required : kRequiredColumns) {
        if (column.count(required) == 0 && !is_optional_column(required)) {
            throw std::runtime_error("path identity ledger is missing required column " +
                                     std::string(required));
        }
    }

    auto field = [&](const std::vector<std::string>& fields, const char* name) -> const std::string& {
        static const std::string absent;
        const auto found = column.find(name);
        return found == column.end() ? absent : fields[found->second];
    };

    PathIdentityLedger ledger;
    // The lookup table is construction-only. Each returned value keeps its immutable
    // storage alive, so neither rows nor Parent pointers depend on the pool's lifetime.
    pancollapse::CompactIdentityTextPool texts;
    std::unordered_map<pancollapse::CompactIdentityText, const AnnotationIdentity*>
        first_exon_by_canonical;
    auto normalized_source = [&](const AnnotationIdentity& identity) {
        if (identity.source_gene.empty() || identity.source_gene == "N/A") {
            return pancollapse::CompactIdentityText{};
        }
        auto found = ledger.normalized_source_genes.find(identity.source_gene);
        if (found == ledger.normalized_source_genes.end()) {
            found = ledger.normalized_source_genes.emplace(
                identity.source_gene, texts.intern(source_gene_identity(identity))).first;
        }
        return found->second;
    };
    ledger.source_path = filename;
    std::string line;
    size_t line_number = 1;
    while (std::getline(in, line)) {
        ++line_number;
        if (line.empty()) {
            throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                     " is empty");
        }
        const std::vector<std::string> fields = split_tab(line);
        if (fields.size() != header.size()) {
            throw std::runtime_error("path identity ledger line " + std::to_string(line_number) +
                                     " has " + std::to_string(fields.size()) +
                                     " columns; expected " + std::to_string(header.size()));
        }
        for (size_t i = 0; i < fields.size(); ++i) {
            require_tsv_safe(fields[i], header[i], line_number);
        }
        for (const char* required : kRequiredColumns) {
            if (is_optional_column(required)) {
                continue;
            }
            if (field(fields, required).empty()) {
                throw std::runtime_error("path identity ledger line " +
                                         std::to_string(line_number) + " has empty required field " +
                                         required);
            }
        }
        if (field(fields, "schema_version") != kSchemaVersion) {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) + " has schema_version " +
                                     field(fields, "schema_version") + "; expected " +
                                     kSchemaVersion);
        }
        const std::string& layer = field(fields, "feature_layer");
        if (layer != "exon" && layer != "body") {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) +
                                     " feature_layer must be exon or body");
        }
        const std::string& strand = field(fields, "strand");
        if (strand != "+" && strand != "-") {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) + " strand must be + or -");
        }
        // These values are emitted directly in the typed-union production BAM. A literal dot is
        // reserved there for a field absent from the other row family.
        for (const char* bam_field : {"vg_path_name", "unique_parent", "canonical_transcript",
                                      "gene_id"}) {
            require_not_bam_missing_sentinel(field(fields, bam_field), bam_field, line_number);
        }

        auto audit = std::make_unique<AnnotationAudit>(AnnotationAudit{
            texts.intern(field(fields, "source_parent")),
            texts.intern(field(fields, "input_parent")),
            texts.intern(field(fields, "haplotype")),
            texts.intern(field(fields, "annotation_source")),
            texts.intern(field(fields, "selection_status")),
            texts.intern(field(fields, "fallback_status")),
            texts.intern(field(fields, "source_transcript")),
            texts.intern(field(fields, "transcript_class")),
        });
        AnnotationIdentity annotation{
            texts.intern(field(fields, "unique_parent")),
            texts.intern(field(fields, "canonical_transcript")),
            texts.intern(field(fields, "gene_id")),
            texts.intern(field(fields, "source_path_or_contig")),
            texts.intern(field(fields, "sample")),
            texts.intern(layer),
            texts.intern(field(fields, "exon_unique_parent")),
            texts.intern(field(fields, "gene_locus")),
            texts.intern(field(fields, "source_gene")),
            texts.intern(strand),
            texts.intern(field(fields, "start")),
            texts.intern(field(fields, "end")),
            std::move(audit),
        };

        if (annotation.canonical_transcript == "N/A" || annotation.gene_id == "N/A") {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) +
                                     " canonical_transcript and gene_id must be resolved identities");
        }
        const std::string& fallback_target =
            layer == "exon" ? annotation.unique_parent : annotation.exon_unique_parent;
        if (annotation.audit->source_transcript == "N/A" &&
            (annotation.canonical_transcript != fallback_target ||
             annotation.audit->fallback_status == "N/A" ||
             annotation.audit->fallback_status == "none")) {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) +
                                     " missing source_transcript must use its exon Parent as "
                                     "canonical with an explicit fallback_status");
        }
        if (layer == "exon" && annotation.exon_unique_parent != annotation.unique_parent) {
            throw std::runtime_error("path identity ledger exon Parent " +
                                     annotation.unique_parent +
                                     " must self-reference exon_unique_parent");
        }
        const uint64_t start =
            parse_positive_coordinate(annotation.start, "start", line_number);
        const uint64_t end = parse_positive_coordinate(annotation.end, "end", line_number);
        if (start > end) {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) + " has start greater than end");
        }

        const std::string& path_name = field(fields, "vg_path_name");
        require_group_component_safe(path_name, "vg_path_name", line_number, true);
        require_group_component_safe(annotation.unique_parent, "unique_parent", line_number, true);
        require_group_component_safe(annotation.canonical_transcript, "canonical_transcript",
                                     line_number, false);
        require_group_component_safe(annotation.gene_id, "gene_id", line_number, false);
        PathIdentityRow row{std::move(annotation), texts.intern(path_name),
                            parse_positive_length(field(fields, "vg_path_length"), line_number)};
        const auto shared_path_name = row.vg_path_name;
        auto [stored_row, row_inserted] =
            ledger.rows_by_path.emplace(shared_path_name, std::move(row));
        if (!row_inserted) {
            throw std::runtime_error("path identity ledger repeats vg_path_name " + path_name);
        }
        const AnnotationIdentity& stored_annotation = stored_row->second.annotation;

        const auto prior_parent = ledger.identities_by_parent.find(
            stored_annotation.unique_parent);
        if (prior_parent != ledger.identities_by_parent.end() &&
            prior_parent->second->feature_layer != stored_annotation.feature_layer) {
            throw std::runtime_error("path identity ledger unique_parent " +
                                     stored_annotation.unique_parent +
                                     " appears in both exon and body layers");
        }
        auto [parent, parent_inserted] =
            ledger.identities_by_parent.emplace(stored_annotation.unique_parent,
                                                &stored_annotation);
        if (!parent_inserted && !(*parent->second == stored_annotation)) {
            throw std::runtime_error("path identity ledger maps unique_parent " +
                                     stored_annotation.unique_parent +
                                     " to multiple identities");
        }
        // Exact Ex50 preflight has a more specific fail-closed diagnostic for a linked body that
        // changes its exon Parent's counted gene. Defer only that body-side contradiction until
        // the graph-aware preflight; exon and ordinary-mode validation remain strict here.
        if (source_gene_identity && layer == "exon") {
            const auto [first, inserted] = first_exon_by_canonical.emplace(
                stored_annotation.canonical_transcript, &stored_annotation);
            if (!inserted) {
                const auto& prior = *first->second;
                if (prior.source_gene != stored_annotation.source_gene ||
                    prior.gene_id != stored_annotation.gene_id) {
                    const auto prior_source = normalized_source(prior);
                    const auto current_source = normalized_source(stored_annotation);
                    if (prior_source.empty() || current_source.empty()) {
                        throw std::runtime_error("cannot qualify conflicting transcript " +
                            stored_annotation.canonical_transcript + " without explicit source_gene");
                    }
                    if (prior_source == current_source && prior.gene_id != stored_annotation.gene_id) {
                        throw std::runtime_error("transcript/source-gene pair maps to multiple raw genes: " +
                            stored_annotation.canonical_transcript + " / " + current_source);
                    }
                    if (prior_source != current_source) {
                        auto& targets = ledger.qualified_targets[stored_annotation.canonical_transcript];
                        for (const auto* candidate : {&prior, &stored_annotation}) {
                            const auto source = normalized_source(*candidate);
                            auto [target, added] = targets.emplace(source, QualifiedTranscriptTarget{
                                {}, source, candidate->gene_id});
                            if (!added && target->second.gene_id != candidate->gene_id) {
                                throw std::runtime_error("transcript/source-gene pair maps to multiple raw genes: " +
                                    candidate->canonical_transcript + " / " + source);
                            }
                        }
                    }
                }
            }
        } else if (!source_gene_identity && !(defer_linked_body_gene_mismatch && layer == "body")) {
            auto [canonical, canonical_inserted] =
                ledger.canonical_gene.emplace(stored_annotation.canonical_transcript,
                                              stored_annotation.gene_id);
            if (!canonical_inserted && canonical->second != stored_annotation.gene_id) {
                throw std::runtime_error("path identity ledger maps canonical transcript " +
                                         stored_annotation.canonical_transcript +
                                         " to multiple genes: " + canonical->second + " versus " +
                                         stored_annotation.gene_id + " at path " +
                                         stored_row->second.vg_path_name + "; source_gene=" +
                                         stored_annotation.source_gene);
            }
        }
        ledger.has_body_layer = ledger.has_body_layer || layer == "body";
        if (progress && ledger.rows_by_path.size() % 1000000 == 0) {
            progress(ledger.rows_by_path.size(), ledger.identities_by_parent.size(),
                     static_cast<uint64_t>(in.tellg()));
        }
    }

    if (ledger.rows_by_path.empty()) {
        throw std::runtime_error("path identity ledger has no data rows");
    }

    bool has_exon = false;
    for (const auto& [path_name, row] : ledger.rows_by_path) {
        const AnnotationIdentity& annotation = row.annotation;
        if (annotation.feature_layer == "exon") {
            has_exon = true;
            continue;
        }
        const auto exon = ledger.identities_by_parent.find(annotation.exon_unique_parent);
        if (exon == ledger.identities_by_parent.end()) {
            throw std::runtime_error("path identity ledger body Parent " +
                                     annotation.unique_parent + " links to missing exon Parent " +
                                     annotation.exon_unique_parent);
        }
        if (exon->second->feature_layer != "exon") {
            throw std::runtime_error("path identity ledger body Parent " +
                                     annotation.unique_parent + " links to non-exon Parent " +
                                     annotation.exon_unique_parent);
        }
        const AnnotationIdentity& exon_identity = *exon->second;
        if (*exon_identity.audit != *annotation.audit ||
            exon_identity.canonical_transcript != annotation.canonical_transcript ||
            exon_identity.source_path_or_contig != annotation.source_path_or_contig ||
            exon_identity.sample != annotation.sample ||
            exon_identity.gene_locus != annotation.gene_locus ||
            exon_identity.source_gene != annotation.source_gene ||
            exon_identity.strand != annotation.strand) {
            throw std::runtime_error("path identity ledger body Parent " +
                                     annotation.unique_parent +
                                     " disagrees with its exon Parent provenance");
        }
        if (exon_identity.gene_id != annotation.gene_id &&
            !defer_linked_body_gene_mismatch) {
            throw std::runtime_error("path identity ledger body Parent " +
                                     annotation.unique_parent +
                                     " disagrees with its exon Parent provenance");
        }
    }
    if (!has_exon) {
        throw std::runtime_error("path identity ledger has no exon rows");
    }
    if (source_gene_identity) {
        for (auto& [canonical, targets] : ledger.qualified_targets) {
            for (auto& [source, target] : targets) {
                // Length framing is injective even if identifiers contain colons.
                target.target = texts.intern(std::string(kQualifiedTargetPrefix) + "Q" +
                    std::to_string(canonical.size()) + ":" + canonical +
                    std::to_string(source.size()) + ":" + source);
                ledger.original_transcripts.emplace(target.target, canonical);
            }
        }
        for (const auto& [canonical, first] : first_exon_by_canonical) {
            static_cast<void>(first);
            if (canonical.view().starts_with(kQualifiedTargetPrefix) &&
                ledger.qualified_targets.count(canonical) == 0) {
                const auto escaped = texts.intern(std::string(kQualifiedTargetPrefix) + "E" +
                    std::to_string(canonical.size()) + ":" + canonical);
                ledger.escaped_targets.emplace(canonical, escaped);
                ledger.original_transcripts.emplace(escaped, canonical);
            }
        }
        if (ledger.qualified_targets.empty()) ledger.normalized_source_genes.clear();
    }
    // All source and cross-layer checks have passed. Preserve the full ledger on
    // disk, not a second resident provenance database throughout counting.
    uint32_t rank = 0;
    for (auto& [path_name, row] : ledger.rows_by_path) {
        static_cast<void>(path_name);
        row.annotation.audit.reset();
        row.lexical_rank = rank++;
    }
    if (ledger.rows_by_path.size() <= std::numeric_limits<uint32_t>::max()) {
        // AnnotationIdentity is the pointer-interconvertible first member. Save
        // representative ranks before moving rows so no dangling pointer is read.
        static_assert(std::is_standard_layout_v<PathIdentityRow>);
        static_assert(offsetof(PathIdentityRow, annotation) == 0);
        std::vector<uint32_t> parent_rows;
        parent_rows.reserve(ledger.identities_by_parent.size());
        for (const auto& [parent, identity] : ledger.identities_by_parent) {
            static_cast<void>(parent);
            parent_rows.push_back(reinterpret_cast<const PathIdentityRow*>(identity)->lexical_rank);
        }
        ledger.rows_by_path.freeze();
        size_t parent_rank = 0;
        ledger.identities_by_parent.freeze([&](const AnnotationIdentity*& identity) {
            identity = &ledger.rows_by_path.at_rank(parent_rows.at(parent_rank++)).second.annotation;
        });
    }
    return ledger;
}

}  // namespace path_identity
