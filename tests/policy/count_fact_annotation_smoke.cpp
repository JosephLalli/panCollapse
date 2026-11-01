// Drives the annotation fact authority end to end: CountFactCatalog::from_annotation builds a
// catalog from a corrected GFF3, a reference GFF3 and a path-identity ledger with no bundle in
// the loop, and that catalog is written through write_count_outputs. D088 records the route;
// this test exists because the route previously reached the output writer for the first time on
// a production run, where an unset provenance field aborted the write after all counting work.
#include "direct_count_facts.hpp"
#include "direct_count_output.hpp"

#include <simdjson.h>
#include <zlib.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace pancollapse::direct_count;

namespace {

void write_file(const std::filesystem::path& path, const std::string& contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << contents;
    if (!output) {
        throw std::runtime_error("cannot write fixture");
    }
}

void write_gzip(const std::filesystem::path& path, const std::string& contents) {
    gzFile output = gzopen(path.c_str(), "wb");
    if (output == nullptr || gzwrite(output, contents.data(), static_cast<unsigned int>(contents.size())) !=
                              static_cast<int>(contents.size()) || gzclose(output) != Z_OK) {
        throw std::runtime_error("cannot write gzip fixture");
    }
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Transcript rows are keyed on ID, which must equal the ledger's unique_parent; the gene is
// reached through Parent. Never key on transcript_id or a transcript row's gene_id.
std::string transcript_row(const std::string& id, const std::string& gene,
                           const std::string& source_gene, const std::string& tags) {
    return "chr1\tfixture\ttranscript\t1\t100\t.\t+\t.\tID=" + id + ";Parent=" + gene +
           ";source_gene=" + source_gene +
           ";gene_type=protein_coding;transcript_type=protein_coding" +
           (tags.empty() ? "" : ";tag=" + tags) + "\n";
}

}  // namespace

int main() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("pancollapse-annotation-facts-" + std::to_string(nonce));
    try {
        const std::string path_header =
            "schema_version\tvg_path_name\tvg_path_length\tvg_haplotype_origins\t"
            "unique_parent\tsource_parent\tinput_parent\tcanonical_transcript\tgene_id\t"
            "source_path_or_contig\tsample\thaplotype\tannotation_source\tfeature_layer\t"
            "selection_status\tfallback_status\texon_unique_parent\tgene_locus\tsource_gene\t"
            "source_transcript\ttranscript_class\tstrand\tstart\tend\n";
        const std::string common =
            "\tsource-parent\tinput-parent\tTX1\tRUNTIME1\tchr1\tSAMPLE\t1\tCAT\t";
        const std::string exon =
            "panSC-path-identity-v1\texon-path\t100\torigin\texon-parent" + common +
            "exon\tselected\tnone\texon-parent\tENSG000001.2\tENSG000001.2\tENST1\t"
            "ortholog\t+\t1\t100\n";
        const std::string body =
            "panSC-path-identity-v1\tbody-path\t200\torigin\tbody-parent" + common +
            "body\tselected\tnone\texon-parent\tENSG000001.2\tENSG000001.2\tENST1\t"
            "ortholog\t+\t1\t200\n";
        const std::filesystem::path ledger_path = root / "path_identity_ledger.tsv";
        write_file(ledger_path, path_header + exon + body);

        // The reference carries a tag that the corrected annotation does not: CAT drops a
        // reference gene's tags when it projects a model, and the route restores them by
        // following source_gene. The version suffix differs on purpose, because a projected
        // source_gene carries one and the reference row need not agree on it.
        const std::filesystem::path reference_path = root / "reference.gff3";
        write_file(reference_path,
                   "##gff-version 3\n"
                   "chr1\tfixture\tgene\t1\t100\t.\t+\t.\tID=ENSG000001.7;"
                   "gene_type=protein_coding;tag=overlaps_pseudogene\n");

        const std::filesystem::path annotation_path = root / "corrected.gff3";
        write_file(annotation_path,
                   "##gff-version 3\n"
                   "chr1\tfixture\tgene\t1\t200\t.\t+\t.\tID=GENE1;"
                   "gene_type=protein_coding;nested_host_gene=ENSG000003\n" +
                       transcript_row("exon-parent", "GENE1", "ENSG000001.2", "") +
                       transcript_row("body-parent", "GENE1", "ENSG000001.2", ""));

        const path_identity::PathIdentityLedger path_ledger =
            path_identity::read(ledger_path, true);
        require(path_ledger.source_path == ledger_path,
                "the ledger did not record the file it was read from");

        const std::vector<ProfileId> profiles{ProfileId::cr7_v1, ProfileId::pansc_strict_v1};
        const CountFactCatalog catalog = CountFactCatalog::from_annotation(
            annotation_path, reference_path, path_ledger, profiles);

        // GENCODE GTF has quoted attributes, permits repeated keys, and can contain semicolons
        // and escapes inside a quoted value. GTF transcript_id/gene_id must produce the same
        // ledger-facing candidates as the equivalent GFF3 rows.
        const std::string gtf =
            "chr1\tfixture\tgene\t1\t200\t.\t+\t.\tgene_id \"GENE1\"; gene_type \"protein_coding\"; "
            "tag \"label_conflict\"; note \"semi; and \\\"quote\\\"\";\n"
            "chr1\tfixture\ttranscript\t1\t100\t.\t+\t.\tgene_id \"GENE1\"; transcript_id \"exon-parent\"; "
            "source_gene \"ENSG000001.2\"; gene_type \"protein_coding\"; transcript_type \"protein_coding\"; "
            "tag \"repeated_tag\"; tag \"another_tag\";\n"
            "chr1\tfixture\ttranscript\t1\t200\t.\t+\t.\tgene_id \"GENE1\"; transcript_id \"body-parent\"; "
            "source_gene \"ENSG000001.2\"; gene_type \"protein_coding\"; transcript_type \"protein_coding\";\n";
        const std::filesystem::path gtf_path = root / "corrected.gtf";
        write_file(gtf_path, gtf);
        const CountFactCatalog gtf_catalog = CountFactCatalog::from_annotation(
            gtf_path, reference_path, path_ledger, profiles, AnnotationFormat::gtf);
        require(gtf_catalog.parents().size() == catalog.parents().size() &&
                    gtf_catalog.profile_gene(ProfileId::cr7_v1, "ENSG000001").competition ==
                        catalog.profile_gene(ProfileId::cr7_v1, "ENSG000001").competition,
                "equivalent GTF did not produce the GFF ledger candidates");
        const ParentFact gtf_exon = gtf_catalog.parent("exon-parent");
        require(std::find(gtf_exon.raw_categories.begin(), gtf_exon.raw_categories.end(),
                          "label_conflict") != gtf_exon.raw_categories.end(),
                "GTF repeated/quoted attributes did not retain the gene tag");

        const std::filesystem::path gtf_gz = root / "corrected.gtf.gz";
        const std::filesystem::path gff_gz = root / "corrected.gff3.gz";
        write_gzip(gtf_gz, gtf);
        write_gzip(gff_gz,
                   "##gff-version 3\r\n"
                   "chr1\tfixture\tgene\t1\t200\t.\t+\t.\tID=GENE1;gene_type=protein_coding\r\n" +
                       transcript_row("exon-parent", "GENE1", "ENSG000001.2", "") +
                       transcript_row("body-parent", "GENE1", "ENSG000001.2", ""));
        require(CountFactCatalog::from_annotation(gtf_gz, reference_path, path_ledger, profiles,
                                                  AnnotationFormat::gtf).parents().size() == 2 &&
                    CountFactCatalog::from_annotation(gff_gz, reference_path, path_ledger, profiles)
                        .parents().size() == 2,
                "compressed GTF or CRLF GFF was not read");

        const auto rejected = [&](const std::filesystem::path& path, AnnotationFormat format) {
            try {
                static_cast<void>(CountFactCatalog::from_annotation(
                    path, reference_path, path_ledger, profiles, format));
                return false;
            } catch (const std::runtime_error&) {
                return true;
            }
        };
        const std::filesystem::path malformed_gtf = root / "malformed.gtf";
        write_file(malformed_gtf,
                   "chr1\tfixture\tgene\t1\t1\t.\t+\t.\tgene_id \"GENE1;\n");
        require(rejected(malformed_gtf, AnnotationFormat::gtf), "malformed GTF was accepted");
        require(rejected(gtf_path, AnnotationFormat::gff3),
                "GTF accepted under an explicit GFF3 format");
        const std::filesystem::path contradictory_gtf = root / "contradictory.gtf";
        write_file(contradictory_gtf,
                   gtf + "chr1\tfixture\ttranscript\t1\t1\t.\t+\t.\tgene_id \"OTHER\"; "
                         "transcript_id \"exon-parent\"; transcript_type \"protein_coding\";\n");
        require(rejected(contradictory_gtf, AnnotationFormat::gtf),
                "GTF duplicate transcript_id with contradictory parent was accepted");
        const std::filesystem::path truncated_gzip = root / "truncated.gff3.gz";
        write_gzip(truncated_gzip,
                   "chr1\tfixture\tgene\t1\t200\t.\t+\t.\tID=GENE1;gene_type=protein_coding\n" +
                       transcript_row("exon-parent", "GENE1", "ENSG000001.2", "") +
                       transcript_row("body-parent", "GENE1", "ENSG000001.2", ""));
        std::filesystem::resize_file(truncated_gzip, std::filesystem::file_size(truncated_gzip) - 6);
        require(rejected(truncated_gzip, AnnotationFormat::gff3), "truncated gzip was accepted");

        require(catalog.authority() == "annotation",
                "the annotation route did not stamp its authority");
        require(catalog.content_id().rfind("sha256:", 0) == 0 &&
                    catalog.content_id().size() == 7 + 64,
                "the derived content id is not a sha256 identifier");
        require(catalog.parents().size() == 2 && catalog.genes().contains("ENSG000001"),
                "the derived catalog did not cover the ledger's Parents");

        // A reference-only tag must reach the Parent, or the annotation is silently under-tagged
        // wherever CAT dropped the gene's tags on projection.
        const ParentFact exon_fact = catalog.parent("exon-parent");
        require(std::find(exon_fact.raw_categories.begin(), exon_fact.raw_categories.end(),
                          "overlaps_pseudogene") != exon_fact.raw_categories.end(),
                "a reference gene tag was not inherited through source_gene");

        // UNKNOWN is not a safe placeholder here: gene competition keeps only PRIMARY
        // candidates under cellranger_2024_a membership, so a blanket UNKNOWN empties the
        // count matrix rather than approximating it.
        const ProfileGeneFact& profile_gene =
            catalog.profile_gene(ProfileId::cr7_v1, "ENSG000001");
        require(profile_gene.competition == CompetitionClass::primary,
                "an allowlisted protein_coding gene was not classified PRIMARY");
        require(profile_gene.nested_host == "ENSG000003",
                "nested_host was not read from the gene row's nested_host_gene attribute");

        // `Parent` is level-dependent. A body row names the exon-layer transcript it was
        // derived from, not the gene, so the locus is two hops up. Half the transcript rows of
        // the production annotation are body rows, and resolving only one hop left every one
        // of them without a gene row -- and so without overlaps_pseudogene and readthrough_gene,
        // which appear on gene rows only.
        {
            const std::filesystem::path path = root / "body.gff3";
            write_file(path,
                       "##gff-version 3\n"
                       "chr1\tfixture\tgene\t1\t200\t.\t+\t.\tID=GENE1;"
                       "gene_type=protein_coding;tag=label_conflict\n" +
                           transcript_row("exon-parent", "GENE1", "ENSG000001.2", "") +
                           // the body row hangs off the exon transcript, as production does
                           transcript_row("body-parent", "exon-parent", "ENSG000001.2", ""));
            const CountFactCatalog derived = CountFactCatalog::from_annotation(
                path, reference_path, path_ledger, profiles);
            const ParentFact body = derived.parent("body-parent");
            // label_conflict is on the gene row and on neither the transcript rows nor
            // the reference gene, so only the gene-row leg can supply it.
            require(std::find(body.raw_categories.begin(), body.raw_categories.end(),
                              "label_conflict") != body.raw_categories.end(),
                    "a body Parent did not reach its gene row through the exon transcript");
        }

        // Readthrough has two signals with different quantifiers. Getting the transcript-level
        // one backwards is not a near miss: with a disjunction, one tagged haplotype copy out
        // of hundreds demotes the locus to FALLBACK, and FALLBACK is discarded outright because
        // gene competition keeps only PRIMARY. On the chr20/21/22 fixture that silently removed
        // 307 genes and 18.2 percent of UMIs. Each case below pins one arm of the rule.
        const auto classify = [&](const std::string& body) {
            const std::filesystem::path path = root / "rt.gff3";
            write_file(path, "##gff-version 3\n" + body);
            const CountFactCatalog derived = CountFactCatalog::from_annotation(
                path, reference_path, path_ledger, profiles);
            const ProfileGeneFact& fact =
                derived.profile_gene(ProfileId::cr7_v1, "ENSG000001");
            return std::pair<CompetitionClass, std::string>{fact.competition,
                                                            fact.competition_reason};
        };
        const std::string plain_gene =
            "chr1\tfixture\tgene\t1\t200\t.\t+\t.\tID=GENE1;gene_type=protein_coding\n";

        // Some but not all observations readthrough: the locus keeps its legitimate isoforms,
        // and filter_categories drops the tagged ones per read. The gene must survive.
        auto partial = classify(
            plain_gene +
            transcript_row("exon-parent", "GENE1", "ENSG000001.2", "readthrough_transcript") +
            transcript_row("body-parent", "GENE1", "ENSG000001.2", ""));
        require(partial.first == CompetitionClass::primary,
                "one readthrough Parent demoted a locus that still has untagged Parents");

        // Every observation readthrough: nothing legitimate remains, so the locus goes.
        auto total = classify(
            plain_gene +
            transcript_row("exon-parent", "GENE1", "ENSG000001.2", "readthrough_transcript") +
            transcript_row("body-parent", "GENE1", "ENSG000001.2", "readthrough_transcript"));
        require(total.first == CompetitionClass::fallback &&
                    total.second == "all_readthrough_transcripts",
                "a wholly readthrough locus was not excluded as all_readthrough_transcripts");

        // A gene-row declaration excludes on its own, whatever the transcripts say.
        auto declared = classify(
            "chr1\tfixture\tgene\t1\t200\t.\t+\t.\tID=GENE1;gene_type=protein_coding;"
            "tag=readthrough_gene\n" +
            transcript_row("exon-parent", "GENE1", "ENSG000001.2", "") +
            transcript_row("body-parent", "GENE1", "ENSG000001.2", ""));
        require(declared.first == CompetitionClass::fallback &&
                    declared.second == "readthrough_gene",
                "a declared readthrough_gene locus was not excluded");

        // The identity must bind every input it was derived from, or a changed annotation
        // reuses an id that no longer describes it.
        write_file(reference_path,
                   "##gff-version 3\n"
                   "chr1\tfixture\tgene\t1\t100\t.\t+\t.\tID=ENSG000001.7;"
                   "gene_type=protein_coding;tag=readthrough_gene\n");
        const CountFactCatalog changed_reference = CountFactCatalog::from_annotation(
            annotation_path, reference_path, path_ledger, profiles);
        require(changed_reference.content_id() != catalog.content_id(),
                "the derived content id did not change when the reference annotation changed");

        CountRuntimeOptions runtime_options;
        runtime_options.raw_barcode_length = 4;
        runtime_options.raw_umi_length = 4;
        CountRuntime runtime({effective_profile(ProfileId::cr7_v1)}, {"ACGT"},
                             {"ENSG000001"}, runtime_options);
        auto worker = runtime.make_worker();
        worker.observe_barcode("ACGT");
        worker.observe_assignment(0, "ACGT", std::nullopt, "TGCA", {"ENSG000001"}, 2);
        worker.flush();
        const CountRuntimeResult count_result = runtime.finalize();

        CountOutputOptions output_options;
        output_options.output_directory = root / "output";
        output_options.pancollapse_version = "0.10.0-test";
        output_options.command_line = "panCollapse count --annotation fixture";
        output_options.fact_bundle_content_id = catalog.content_id();
        output_options.fact_authority = catalog.authority();
        output_options.threads = 2;
        const CountOutputReceipt receipt =
            write_count_outputs(runtime, count_result, catalog, output_options);
        require(receipt.profile_logical_tables.at("cr7-v1").counts_sha256.size() == 64,
                "the annotation route did not produce a counts digest");
        require(std::filesystem::is_regular_file(root / "output/parquet/counts.parquet"),
                "the annotation route did not write counts.parquet");

        simdjson::dom::parser json_parser;
        simdjson::dom::element output_manifest;
        require(!json_parser.load((root / "output/manifest.json").string())
                     .get(output_manifest),
                "the annotation route did not write a valid manifest");
        require(std::string_view(output_manifest["fact_authority"]) == "annotation",
                "the receipt did not record the annotation fact authority");
        require(std::string_view(output_manifest["fact_bundle_content_id"]) ==
                    catalog.content_id(),
                "the receipt did not record the derived fact content id");

        // The write must refuse an unstamped authority rather than emit an unattributable
        // receipt; this is the check the production run tripped, with the field left empty.
        CountOutputOptions unstamped = output_options;
        unstamped.output_directory = root / "unstamped-output";
        unstamped.fact_authority.clear();
        bool unstamped_rejected = false;
        try {
            static_cast<void>(
                write_count_outputs(runtime, count_result, catalog, unstamped));
        } catch (const std::invalid_argument&) {
            unstamped_rejected = true;
        }
        require(unstamped_rejected, "an unstamped fact authority was accepted");

        std::filesystem::remove_all(root);
        std::cout << "count_fact_annotation_smoke passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
