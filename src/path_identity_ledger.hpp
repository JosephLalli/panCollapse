#pragma once

// Strict reader for the production panSC post-vg path identity ledger.  This is
// intentionally separate from the historical 2/3-column t2g adapter: callers
// select the schema explicitly and no file-shape auto-detection occurs.

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace path_identity {

inline constexpr const char* kSchemaVersion = "panSC-path-identity-v1";

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

struct AnnotationIdentity {
    std::string schema_version;
    std::string unique_parent;
    std::string source_parent;
    std::string input_parent;
    std::string canonical_transcript;
    std::string gene_id;
    std::string source_path_or_contig;
    std::string sample;
    std::string haplotype;
    std::string annotation_source;
    std::string feature_layer;
    std::string selection_status;
    std::string fallback_status;
    std::string exon_unique_parent;
    std::string gene_locus;
    std::string source_gene;
    std::string source_transcript;
    std::string transcript_class;
    std::string strand;
    std::string start;
    std::string end;

    bool operator==(const AnnotationIdentity&) const = default;
};

struct PathIdentityRow {
    AnnotationIdentity annotation;
    std::string vg_path_name;
    uint64_t vg_path_length = 0;
    std::string vg_haplotype_origins;
};

struct PathIdentityLedger {
    std::map<std::string, PathIdentityRow> rows_by_path;
    std::map<std::string, AnnotationIdentity> identities_by_parent;
    std::map<std::string, std::string> canonical_gene;
    bool has_body_layer = false;
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
                               bool defer_linked_body_gene_mismatch = false) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open path identity ledger " + filename.string());
    }

    std::string header_line;
    if (!std::getline(in, header_line) || header_line.empty()) {
        throw std::runtime_error("path identity ledger is missing its header");
    }
    const std::vector<std::string> header = split_tab(header_line);
    if (header.size() != kRequiredColumns.size()) {
        throw std::runtime_error("path identity ledger header does not exactly match schema " +
                                 std::string(kSchemaVersion));
    }
    for (size_t i = 0; i < kRequiredColumns.size(); ++i) {
        if (header[i] != kRequiredColumns[i]) {
            throw std::runtime_error("path identity ledger header column " +
                                     std::to_string(i + 1) + " is " + header[i] +
                                     "; expected " + kRequiredColumns[i]);
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
        if (column.count(required) == 0) {
            throw std::runtime_error("path identity ledger is missing required column " +
                                     std::string(required));
        }
    }

    auto field = [&](const std::vector<std::string>& fields, const char* name) -> const std::string& {
        return fields[column.at(name)];
    };

    PathIdentityLedger ledger;
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

        AnnotationIdentity annotation{
            field(fields, "schema_version"),
            field(fields, "unique_parent"),
            field(fields, "source_parent"),
            field(fields, "input_parent"),
            field(fields, "canonical_transcript"),
            field(fields, "gene_id"),
            field(fields, "source_path_or_contig"),
            field(fields, "sample"),
            field(fields, "haplotype"),
            field(fields, "annotation_source"),
            layer,
            field(fields, "selection_status"),
            field(fields, "fallback_status"),
            field(fields, "exon_unique_parent"),
            field(fields, "gene_locus"),
            field(fields, "source_gene"),
            field(fields, "source_transcript"),
            field(fields, "transcript_class"),
            strand,
            field(fields, "start"),
            field(fields, "end"),
        };

        if (annotation.canonical_transcript == "N/A" || annotation.gene_id == "N/A") {
            throw std::runtime_error("path identity ledger line " +
                                     std::to_string(line_number) +
                                     " canonical_transcript and gene_id must be resolved identities");
        }
        const std::string& fallback_target =
            layer == "exon" ? annotation.unique_parent : annotation.exon_unique_parent;
        if (annotation.source_transcript == "N/A" &&
            (annotation.canonical_transcript != fallback_target ||
             annotation.fallback_status == "N/A" ||
             annotation.fallback_status == "none")) {
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
        PathIdentityRow row{annotation, path_name,
                            parse_positive_length(field(fields, "vg_path_length"), line_number),
                            field(fields, "vg_haplotype_origins")};
        if (!ledger.rows_by_path.emplace(path_name, row).second) {
            throw std::runtime_error("path identity ledger repeats vg_path_name " + path_name);
        }

        const auto prior_parent = ledger.identities_by_parent.find(annotation.unique_parent);
        if (prior_parent != ledger.identities_by_parent.end() &&
            prior_parent->second.feature_layer != annotation.feature_layer) {
            throw std::runtime_error("path identity ledger unique_parent " +
                                     annotation.unique_parent +
                                     " appears in both exon and body layers");
        }
        auto [parent, parent_inserted] =
            ledger.identities_by_parent.emplace(annotation.unique_parent, annotation);
        if (!parent_inserted && !(parent->second == annotation)) {
            throw std::runtime_error("path identity ledger maps unique_parent " +
                                     annotation.unique_parent + " to multiple identities");
        }
        // Exact Ex50 preflight has a more specific fail-closed diagnostic for a linked body that
        // changes its exon Parent's counted gene. Defer only that body-side contradiction until
        // the graph-aware preflight; exon and ordinary-mode validation remain strict here.
        if (!(defer_linked_body_gene_mismatch && layer == "body")) {
            auto [canonical, canonical_inserted] =
                ledger.canonical_gene.emplace(annotation.canonical_transcript, annotation.gene_id);
            if (!canonical_inserted && canonical->second != annotation.gene_id) {
                throw std::runtime_error("path identity ledger maps canonical transcript " +
                                         annotation.canonical_transcript + " to multiple genes");
            }
        }
        ledger.has_body_layer = ledger.has_body_layer || layer == "body";
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
        if (exon->second.feature_layer != "exon") {
            throw std::runtime_error("path identity ledger body Parent " +
                                     annotation.unique_parent + " links to non-exon Parent " +
                                     annotation.exon_unique_parent);
        }
        const AnnotationIdentity& exon_identity = exon->second;
        if (exon_identity.source_parent != annotation.source_parent ||
            exon_identity.input_parent != annotation.input_parent ||
            exon_identity.canonical_transcript != annotation.canonical_transcript ||
            exon_identity.source_path_or_contig != annotation.source_path_or_contig ||
            exon_identity.sample != annotation.sample ||
            exon_identity.haplotype != annotation.haplotype ||
            exon_identity.annotation_source != annotation.annotation_source ||
            exon_identity.selection_status != annotation.selection_status ||
            exon_identity.fallback_status != annotation.fallback_status ||
            exon_identity.gene_locus != annotation.gene_locus ||
            exon_identity.source_gene != annotation.source_gene ||
            exon_identity.source_transcript != annotation.source_transcript ||
            exon_identity.transcript_class != annotation.transcript_class ||
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
    return ledger;
}

}  // namespace path_identity
