#include "direct_count_compatibility.hpp"
#include "direct_count_facts.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

using Row = std::vector<std::string>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Callable>
void require_throws(Callable&& callable, const char* message) {
    bool rejected = false;
    try {
        callable();
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, message);
}

Row row(const std::string& parent, const std::string& canonical,
        const std::string& gene, const std::string& source_gene,
        const std::string& path, const std::string& layer = "exon",
        const std::string& exon_parent = "") {
    const std::string exon = exon_parent.empty() ? parent : exon_parent;
    const std::string& biological_parent = layer == "body" ? exon : parent;
    return {path_identity::kSchemaVersion, path, "20", "chr1", parent,
            "source_" + biological_parent, "input_" + biological_parent,
            canonical, gene, "chr1", "sample1", "hap1", "CAT", layer,
            "selected", "none", exon, "chr1:1-20", source_gene, "source_tx",
            "mapped", "+", "1", "20"};
}

std::filesystem::path write_ledger(const std::filesystem::path& root,
                                   const std::string& name,
                                   const std::vector<Row>& rows) {
    const auto path = root / (name + ".tsv");
    std::ofstream out(path);
    require(static_cast<bool>(out), "could not create temporary ledger");
    for (size_t i = 0; i < path_identity::kRequiredColumns.size(); ++i) {
        if (i != 0) out << '\t';
        out << path_identity::kRequiredColumns[i];
    }
    out << '\n';
    for (const Row& fields : rows) {
        require(fields.size() == path_identity::kRequiredColumns.size(),
                "fixture row has unexpected field count");
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i != 0) out << '\t';
            out << fields[i];
        }
        out << '\n';
    }
    return path;
}

std::filesystem::path write_annotation(const std::filesystem::path& root,
                                       const std::string& name) {
    const auto path = root / name;
    std::ofstream out(path);
    require(static_cast<bool>(out), "could not create temporary annotation");
    out << "##gff-version 3\n";
    return path;
}

path_identity::PathIdentityLedger read_qualified(const std::filesystem::path& path) {
    return path_identity::read(
        path, false, {}, pancollapse::direct_count::normalized_count_gene_identity);
}

}  // namespace

int main() {
    static_assert(sizeof(path_identity::PathIdentityRow) <= 136);
    const auto root = std::filesystem::temp_directory_path() /
        ("pancollapse-qualified-targets-smoke-" + std::to_string(getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    try {
        const auto conflict_fixture = std::filesystem::path(__FILE__).parent_path().parent_path() /
            "fixtures/path_identity_conflict_ENST00000477740.tsv";
        require_throws([&] { (void)path_identity::read(conflict_fixture); },
                       "original reader accepted the real canonical/gene conflict");
        const auto fixture_ledger = read_qualified(conflict_fixture);
        require(fixture_ledger.qualified_target_count() == 2,
                "qualified reader did not split the real conflict fixture");
        const auto& fixture_identity = fixture_ledger.rows_by_path
            .at("chr2::CHM13_T0135505_R1").annotation;
        const std::string fixture_target = fixture_ledger.target_transcript(fixture_identity);
        require(fixture_ledger.original_transcript(fixture_target) == "ENST00000477740" &&
                    fixture_identity.canonical_transcript == "ENST00000477740" &&
                    fixture_identity.gene_id == "ENSG00000238009" &&
                    fixture_identity.source_gene == "ENSG00000238009.6",
                "qualified targets did not retain original row labels and reverse mapping");

        const std::vector<Row> split_rows = {
            row("P_A", "TX", "RAW_GENE", "source-A.1", "path-A"),
            row("P_B", "TX", "RAW_GENE", "source-B.2", "path-B"),
        };
        const auto split = write_ledger(root, "split", split_rows);
        const auto shuffled = write_ledger(root, "shuffled", {split_rows[1], split_rows[0]});
        const auto qualified = read_qualified(split);
        const auto reordered = read_qualified(shuffled);
        const auto formerly_merged = path_identity::read(split);
        require(pancollapse::direct_count::compatibility_structural_surface_id(formerly_merged) !=
                    pancollapse::direct_count::compatibility_structural_surface_id(qualified),
                "source-gene partition reused the old merged replay identity");
        require(qualified.qualified_target_count() == 2 &&
                    qualified.qualified_targets.at("TX").size() == 2 &&
                    qualified.rows_by_path.at("path-A").annotation.gene_id == "RAW_GENE" &&
                    qualified.rows_by_path.at("path-B").annotation.gene_id == "RAW_GENE",
                "different normalized sources did not split one raw-gene canonical");
        require(pancollapse::direct_count::qualified_transcript_mapping_id(qualified) ==
                    pancollapse::direct_count::qualified_transcript_mapping_id(reordered) &&
                    pancollapse::direct_count::compatibility_structural_surface_id(qualified) ==
                    pancollapse::direct_count::compatibility_structural_surface_id(reordered),
                "qualified mapping keys or structural hash depend on ledger row order");
        const auto& target_a = qualified.target_transcript(
            qualified.rows_by_path.at("path-A").annotation);
        const auto& target_b = qualified.target_transcript(
            qualified.rows_by_path.at("path-B").annotation);
        require(target_a != target_b && qualified.original_transcript(target_a) == "TX" &&
                    qualified.original_transcript(target_b) == "TX",
                "split targets do not preserve canonical transcript provenance");

        const auto annotation = write_annotation(root, "annotation.gff3");
        const auto reference = write_annotation(root, "reference.gff3");
        const auto catalog = pancollapse::direct_count::CountFactCatalog::from_annotation(
            annotation, reference, qualified,
            {pancollapse::direct_count::ProfileId::cr7_v1});
        pancollapse::direct_count::CompatibilityFactSet accepted;
        accepted.exact = {{"TX", "P_A", "path-A", "P_A", 1,
                           pancollapse::direct_count::EvidenceTier::exon,
                           pancollapse::direct_count::EvidenceStrand::forward}};
        accepted.structural = {{"TX", pancollapse::direct_count::StructuralLayer::spliced,
                                1, pancollapse::direct_count::EvidenceStrand::forward,
                                {"path-A"}, {"P_A"}}};
        catalog.validate_compatibility_structure(accepted, qualified);
        auto mixed_exact = accepted;
        mixed_exact.exact.front().locus_parent = "P_B";
        require_throws([&] { catalog.validate_compatibility_structure(mixed_exact, qualified); },
                       "exact compatibility fact crossed source-gene targets");
        auto mixed_structural = accepted;
        mixed_structural.structural.front().winning_paths = {"path-A", "path-B"};
        mixed_structural.structural.front().winning_parents = {"P_A", "P_B"};
        require_throws([&] {
            catalog.validate_compatibility_structure(mixed_structural, qualified);
        }, "structural compatibility fact crossed source-gene targets");

        require_throws([&] {
            (void)read_qualified(write_ledger(root, "same-normalized-source", {
                row("P1", "TX", "RAW1", "ENSG00000000001.1", "path-1"),
                row("P2", "TX", "RAW2", "ENSG00000000001.2", "path-2"),
            }));
        }, "different raw genes under one normalized source were accepted");
        require_throws([&] {
            (void)read_qualified(write_ledger(root, "missing-source", {
                row("P1", "TX", "RAW1", "source-A", "path-1"),
                row("P2", "TX", "RAW2", "N/A", "path-2"),
            }));
        }, "conflicting canonical without source_gene was accepted");
        require_throws([&] {
            (void)read_qualified(write_ledger(root, "repeated-parent", {
                row("P", "TX1", "G1", "source-A", "path-1"),
                row("P", "TX2", "G2", "source-B", "path-2"),
            }));
        }, "conflicting repeated Parent was accepted in qualified mode");

        require_throws([&] {
            (void)read_qualified(write_ledger(root, "missing-body-crosslink", {
                row("P", "TX", "G", "source-A", "exon-path"),
                row("BODY", "TX", "G", "source-A", "body-path", "body", "MISSING"),
            }));
        }, "qualified mode accepted a missing body/exon crosslink");
        auto bad_provenance = row("BODY", "TX", "G", "source-A", "body-path", "body", "P");
        bad_provenance[10] = "other-sample";
        require_throws([&] {
            (void)read_qualified(write_ledger(root, "bad-body-provenance", {
                row("P", "TX", "G", "source-A", "exon-path"), bad_provenance,
            }));
        }, "qualified mode accepted body/exon provenance disagreement");

        const std::string reserved = "panSCtxg1:Q3:TX8:source-A";
        const auto reserved_ledger = read_qualified(write_ledger(root, "reserved-prefix", {
            row("P", reserved, "G", "source-A", "reserved-path"),
        }));
        const auto& reserved_target = reserved_ledger.target_transcript(
            reserved_ledger.rows_by_path.at("reserved-path").annotation);
        require(reserved_target != reserved &&
                    reserved_ledger.original_transcript(reserved_target) == reserved,
                "reserved-prefix canonical transcript was not uniquely escaped");

        const auto nonconflict = write_ledger(root, "nonconflict", {
            row("P", "TX", "G", "source-A", "path"),
        });
        const auto strict = path_identity::read(nonconflict);
        const auto opted_in = read_qualified(nonconflict);
        require(opted_in.qualified_target_count() == 0 &&
                    pancollapse::direct_count::compatibility_structural_surface_id(strict) ==
                    pancollapse::direct_count::compatibility_structural_surface_id(opted_in),
                "nonconflicting opt-in changed the compatibility structural surface");
    } catch (...) { throw; }
    std::filesystem::remove_all(root);
    return 0;
}
