#include "path_identity_ledger.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using Row = std::vector<std::string>;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

Row row(const std::string& layer, const std::string& parent,
        const std::string& exon_parent, const std::string& canonical,
        const std::string& gene, const std::string& path) {
    const std::string& biological_parent = layer == "body" ? exon_parent : parent;
    return {path_identity::kSchemaVersion,
            path,
            "20",
            "chr1,ALT",
            parent,
            "source_" + biological_parent,
            "input_" + biological_parent,
            canonical,
            gene,
            "chr1",
            "sample1",
            "hap1",
            "CAT",
            layer,
            "selected",
            "none",
            exon_parent,
            "chr1:1-20",
            gene,
            "source_tx",
            "mapped",
            "+",
            "1",
            "20"};
}

std::filesystem::path write_ledger(const std::string& name, const std::vector<Row>& rows,
                                   std::vector<std::string> header = {}) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("pancollapse_" + name + ".tsv");
    std::ofstream out(path);
    if (header.empty()) {
        header.assign(path_identity::kRequiredColumns.begin(),
                      path_identity::kRequiredColumns.end());
    }
    for (size_t i = 0; i < header.size(); ++i) {
        if (i != 0) out << '\t';
        out << header[i];
    }
    out << '\n';
    for (const Row& fields : rows) {
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i != 0) out << '\t';
            out << fields[i];
        }
        out << '\n';
    }
    out.close();
    return path;
}

bool rejects(const std::string& name, const std::vector<Row>& rows) {
    const auto path = write_ledger(name, rows);
    try {
        (void)path_identity::read(path);
    } catch (const std::runtime_error&) {
        std::filesystem::remove(path);
        return true;
    }
    std::filesystem::remove(path);
    return false;
}

void run() {
    // Multiple emitted paths per Parent and multiple Parents per canonical are legal. Literal
    // suffixes are identities, not syntax, and commas in general provenance are legal.
    const std::vector<Row> valid = {
        row("exon", "UP_A", "UP_A", "CANON", "GENE1", "path_A"),
        row("exon", "UP_A", "UP_A", "CANON", "GENE1", "path_A_copy"),
        row("exon", "literal_R1", "literal_R1", "literal_R1", "GENE2", "path_literal"),
        row("exon", "UP_B", "UP_B", "CANON", "GENE1", "path_B"),
        row("body", "BODY_A", "UP_A", "CANON", "GENE1", "body_A"),
    };
    const auto valid_path = write_ledger("valid", valid);
    const auto ledger = path_identity::read(valid_path);
    check(ledger.rows_by_path.size() == 5, "valid: all exact paths retained");
    check(ledger.identities_by_parent.size() == 4,
          "valid: repeated emitted paths share one Parent identity");
    check(ledger.canonical_gene.at("literal_R1") == "GENE2",
          "valid: literal _R1 identity preserved");
    std::filesystem::remove(valid_path);

    {
        auto rows = valid;
        rows.push_back(row("body", "UP_A", "UP_A", "CANON", "GENE1", "bad_overlap"));
        check(rejects("layer_overlap", rows), "reject Parent appearing in both layers");
    }
    {
        auto rows = valid;
        rows[1][5] = "conflicting_source_parent";
        check(rejects("parent_identity_conflict", rows),
              "reject conflicting annotation identity for one Parent");
    }
    {
        auto rows = valid;
        rows.push_back(row("exon", "UP_C", "UP_C", "CANON", "GENE_X", "gene_conflict"));
        check(rejects("gene_conflict", rows), "reject canonical-to-gene conflict");
    }
    {
        auto rows = valid;
        rows.push_back(row("body", "BODY_MISSING", "NO_EXON", "CANON", "GENE1",
                           "missing_crosslink"));
        check(rejects("missing_crosslink", rows), "reject missing body-to-exon crosslink");
    }
    {
        auto rows = valid;
        rows.push_back(row("body", "BODY_WRONG", "UP_A", "literal_R1", "GENE2",
                           "wrong_crosslink"));
        check(rejects("wrong_crosslink", rows),
              "reject body/exon canonical and gene disagreement");
    }
    {
        auto rows = valid;
        rows.back()[10] = "different_sample";
        check(rejects("body_provenance_mismatch", rows),
              "reject body/exon provenance disagreement");
    }
    {
        auto rows = valid;
        rows[0][0] = "future-schema";
        check(rejects("schema", rows), "reject unknown schema version");
    }
    {
        std::vector<std::string> header(path_identity::kRequiredColumns.begin(),
                                        path_identity::kRequiredColumns.end());
        std::swap(header[1], header[4]);
        const auto path = write_ledger("header_order", valid, header);
        bool rejected = false;
        try {
            (void)path_identity::read(path);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        std::filesystem::remove(path);
        check(rejected, "reject noncanonical header order");
    }
    {
        auto rows = valid;
        rows[0][6].clear();
        check(rejects("empty_required", rows), "reject empty required provenance");
    }
    {
        auto rows = valid;
        rows[0][1] = "bad,path";
        check(rejects("path_delimiter", rows), "reject comma in XP path component");
    }
    {
        auto rows = valid;
        rows[0][4] = "bad;parent";
        rows[0][16] = "bad;parent";
        check(rejects("parent_delimiter", rows), "reject semicolon in XU Parent component");
    }
    {
        auto rows = valid;
        rows[0][7] = "bad;canonical";
        check(rejects("canonical_delimiter", rows), "reject semicolon in TX component");
    }
    for (const auto& [index, name] : std::vector<std::pair<size_t, const char*>>{
             {1, "path"}, {4, "parent"}, {7, "canonical"}, {8, "gene"}}) {
        auto rows = valid;
        rows[0][index] = ".";
        check(rejects(std::string("missing_sentinel_") + name, rows),
              "reject typed-union missing-sentinel collision");
    }
    {
        auto rows = valid;
        rows[0][22] = "zero";
        check(rejects("nonnumeric_start", rows), "reject non-numeric start");
    }
    {
        auto rows = valid;
        rows[0][22] = "21";
        check(rejects("reversed_interval", rows), "reject start greater than end");
    }
    {
        Row exon = row("exon", "FALLBACK", "FALLBACK", "FALLBACK", "GENE3", "fallback_exon");
        exon[15] = "missing_source_transcript";
        exon[19] = "N/A";
        Row body = row("body", "FALLBACK_BODY", "FALLBACK", "FALLBACK", "GENE3",
                       "fallback_body");
        body[15] = "missing_source_transcript";
        body[19] = "N/A";
        const auto path = write_ledger("body_fallback", {exon, body});
        const auto fallback = path_identity::read(path);
        check(fallback.rows_by_path.size() == 2,
              "missing-source body falls back through exon_unique_parent");
        std::filesystem::remove(path);
        body[7] = "FALLBACK_BODY";
        check(rejects("bad_body_fallback", {exon, body}),
              "reject missing-source body fallback to its body Parent");
        exon[7] = "FALLBACK";
        body[7] = "FALLBACK";
        exon[15] = "none";
        body[15] = "none";
        check(rejects("missing_source_without_fallback", {exon, body}),
              "reject missing source transcript without an explicit fallback");
    }

    std::printf("path identity ledger: PASS\n");
}

}  // namespace

int main() {
    run();
    return 0;
}
