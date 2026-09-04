#include "direct_count_facts.hpp"
#include "direct_count_output.hpp"

#include <simdjson.h>

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

std::string quote(const std::string& value) {
    return "\"" + value + "\"";
}

std::string file_record(const std::filesystem::path& root, const std::string& relative,
                        const std::string& schema) {
    const std::filesystem::path path = root / relative;
    const std::string digest = sha256_file(path);
    const auto size = std::filesystem::file_size(path);
    return "{\"path\":" + quote(relative) + ",\"schema\":" + quote(schema) +
           ",\"sha256\":" + quote(digest) + ",\"size_bytes\":" +
           std::to_string(size) + ",\"source_sha256\":" + quote(digest) +
           ",\"source_size_bytes\":" + std::to_string(size) + "}";
}

std::string digest_text(const std::filesystem::path& root, const std::string& text) {
    const std::filesystem::path temporary = root / ".digest-input";
    write_file(temporary, text);
    const std::string result = sha256_file(temporary);
    std::filesystem::remove(temporary);
    return result;
}

}  // namespace

int main() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("pancollapse-count-facts-" + std::to_string(nonce));
    try {
        const std::string path_header =
            "schema_version\tvg_path_name\tvg_path_length\tvg_haplotype_origins\t"
            "unique_parent\tsource_parent\tinput_parent\tcanonical_transcript\tgene_id\t"
            "source_path_or_contig\tsample\thaplotype\tannotation_source\tfeature_layer\t"
            "selection_status\tfallback_status\texon_unique_parent\tgene_locus\tsource_gene\t"
            "source_transcript\ttranscript_class\tstrand\tstart\tend\n";
        const std::string common =
            "\tsource-parent\tinput-parent\tTX1\tRUNTIME1\tchr1\tSAMPLE\t1\tCAT\t";
        const std::string exon = "panSC-path-identity-v1\texon-path\t100\torigin\texon-parent" +
                                 common +
                                 "exon\tselected\tnone\texon-parent\tENSG000001.2\t"
                                 "ENSG000001.2\tENST1\tortholog\t+\t1\t100\n";
        const std::string body = "panSC-path-identity-v1\tbody-path\t200\torigin\tbody-parent" +
                                 common +
                                 "body\tselected\tnone\texon-parent\tENSG000001.2\t"
                                 "ENSG000001.2\tENST1\tortholog\t+\t1\t200\n";
        write_file(root / "files/path_identity_ledger.tsv", path_header + exon + body);
        write_file(root / "files/profile_gene_policy.tsv",
                   "profile_id\tcount_gene\tgene_type\tcompetition_class\tcompetition_reason\t"
                   "equivalence_gene\tsource_annotation\tnested_host\n"
                   "cr7-v1\tENSG000001\tprotein_coding\tPRIMARY\tallowed\t"
                   "ENSG000001\tCAT\t.\n"
                   "pansc-strict-v1\tENSG000001\tprotein_coding\tFALLBACK\t"
                   "profile-specific-fixture\tENSG000001\tCAT\t.\n");
        write_file(root / "files/profile_policy_receipt_cr7_v1.json", "{}\n");
        write_file(root / "files/profile_policy_receipt_pansc_strict_v1.json", "{}\n");
        write_file(root / "files/gene_metadata.tsv",
                   "count_gene\tgene_name\tgene_type\n"
                   "ENSG000001\tFIXTURE1\tprotein_coding\n");
        write_file(root / "files/parent_category_ledger.tsv",
                   "unique_parent\tcategories\nbody-parent\t.\nexon-parent\t.\n");
        write_file(root / "files/parent_category_receipt.json",
                   "{\"schema\":\"panSC-parent-category-ledger-v3\"}\n");
        write_file(root / "files/strong_support_ledger.tsv",
                   "unique_parent\tcount_gene\traw_categories\teffective_categories\t"
                   "strong_support_tag\tprotected_primary_protein_host\texempted_categories\n"
                   "body-parent\tENSG000001\t.\t.\tfalse\tfalse\t.\n"
                   "exon-parent\tENSG000001\t.\t.\tfalse\tfalse\t.\n");
        write_file(root / "files/projected_nested_policy.json",
                   "{\"schema\":\"panSC-projected-nested-host-policy-v1\"}\n");
        write_file(root / "files/corrected_annotation.gff3", "##gff-version 3\n");

        const std::string files =
            "{\"corrected_annotation\":" +
            file_record(root, "files/corrected_annotation.gff3", "annotation") +
            ",\"profile_gene_policy\":" +
            file_record(root, "files/profile_gene_policy.tsv",
                        "panSC-count-profile-gene-policy-v1") +
            ",\"profile_policy_receipt_cr7_v1\":" +
            file_record(root, "files/profile_policy_receipt_cr7_v1.json",
                        "panSC-count-cr-gene-policy-only-v2") +
            ",\"profile_policy_receipt_pansc_strict_v1\":" +
            file_record(root, "files/profile_policy_receipt_pansc_strict_v1.json",
                        "panSC-projected-nested-host-policy-v1") +
            ",\"gene_metadata\":" +
            file_record(root, "files/gene_metadata.tsv", "panSC-gene-metadata-v1") +
            ",\"parent_category_ledger\":" +
            file_record(root, "files/parent_category_ledger.tsv",
                        "panSC-parent-category-ledger-v3") +
            ",\"parent_category_receipt\":" +
            file_record(root, "files/parent_category_receipt.json",
                        "panSC-parent-category-ledger-v3") +
            ",\"path_identity_ledger\":" +
            file_record(root, "files/path_identity_ledger.tsv", "panSC-path-identity-v1") +
            ",\"projected_nested_policy\":" +
            file_record(root, "files/projected_nested_policy.json",
                        "panSC-projected-nested-host-policy-v1") +
            ",\"strong_support_ledger\":" +
            file_record(root, "files/strong_support_ledger.tsv",
                        "panSC-strong-support-audit-v1") + "}";
        const std::string content =
            "{\"assignment_contract\":{\"nested_host_interpretation\":"
            "\"relation-owner-loses\",\"profile_policy_source\":"
            "\"profile-specific-certified-ledgers\",\"score_window_order\":"
            "\"before-parent-category-filter\",\"transcript_filter_mode\":"
            "\"parent-category-ledger-v3\"},\"files\":" + files +
            ",\"validation\":{\"gene_policy_genes\":1,"
            "\"parent_category_coverage\":\"exact\",\"path_identity_parents\":2,"
            "\"strong_support_coverage\":\"exact\"}}";
        const std::string manifest =
            "{\"content\":" + content + ",\"content_id\":\"sha256:" +
            digest_text(root, content) +
            "\",\"created_by\":{\"program\":\"fixture\",\"version\":\"1\"},"
            "\"schema\":\"panSC-count-facts-v1\"}\n";
        write_file(root / "MANIFEST.json", manifest);

        const CountFactBundle bundle = load_count_fact_bundle(root);
        const CountFactBundle manifest_bundle =
            load_count_fact_bundle(root / "MANIFEST.json");
        if (manifest_bundle.content_id != bundle.content_id ||
            manifest_bundle.manifest_path != bundle.manifest_path ||
            bundle.transcript_filter_mode != "parent-category-ledger-v3") {
            throw std::runtime_error("manifest-file bundle loading differs from directory loading");
        }
        const path_identity::PathIdentityLedger path_ledger = path_identity::read(
            bundle.files.at("path_identity_ledger").path, true);
        const CountFactCatalog catalog = CountFactCatalog::load(bundle, path_ledger);
        const AssignmentCandidate candidate = catalog.candidate(
            ProfileId::cr7_v1, "exon-parent", 100, EvidenceTier::exon,
            EvidenceStrand::forward);
        const AssignmentCandidate pansc_candidate = catalog.candidate(
            ProfileId::pansc_strict_v1, "exon-parent", 100, EvidenceTier::exon,
            EvidenceStrand::forward);
        if (candidate.gene != "ENSG000001" ||
            candidate.competition != CompetitionClass::primary ||
            pansc_candidate.competition != CompetitionClass::fallback ||
            candidate.gene_types != std::vector<std::string>{"protein_coding"} ||
            catalog.gene("ENSG000001").gene_name !=
                std::optional<std::string>{"FIXTURE1"}) {
            throw std::runtime_error("catalog candidate differs from fixture facts");
        }

        CountRuntimeOptions runtime_options;
        runtime_options.memory_budget_bytes = 1ULL << 20;
        runtime_options.spill_directory = root / "spill";
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
        output_options.command_line = "panCollapse count fixture";
        output_options.fact_bundle_content_id = bundle.content_id;
        output_options.threads = 2;
        output_options.memory_budget_bytes = runtime_options.memory_budget_bytes;
        output_options.write_10x_mex = true;
        const CountOutputReceipt receipt =
            write_count_outputs(runtime, count_result, catalog, output_options);
        if (receipt.tables.size() != 4 ||
            receipt.profile_logical_tables.size() != 1 ||
            receipt.profile_logical_tables.at("cr7-v1").counts_sha256.size() != 64 ||
            receipt.profile_logical_tables.at("cr7-v1").molecules_sha256.size() != 64 ||
            !std::filesystem::is_regular_file(root / "output/parquet/counts.parquet") ||
            !std::filesystem::is_regular_file(
                root / "output/mex/cr7-v1/raw_feature_bc_matrix/matrix.mtx.gz")) {
            throw std::runtime_error("direct-count output bundle is incomplete");
        }
        simdjson::dom::parser json_parser;
        simdjson::dom::element output_manifest;
        if (json_parser.load((root / "output/manifest.json").string()).get(output_manifest) ||
            std::string_view(output_manifest["schema"]) != "pancollapse-count-output-v1") {
            throw std::runtime_error("direct-count manifest is not valid JSON");
        }

        CountOutputOptions invalid_metadata = output_options;
        invalid_metadata.output_directory = root / "invalid-output";
        invalid_metadata.profile_assignment_terminals.resize(2);
        bool terminal_shape_rejected = false;
        try {
            static_cast<void>(write_count_outputs(
                runtime, count_result, catalog, invalid_metadata));
        } catch (const std::invalid_argument&) {
            terminal_shape_rejected = true;
        }
        if (!terminal_shape_rejected) {
            throw std::runtime_error(
                "mismatched profile terminal counters were accepted");
        }

        write_file(root / "files/parent_category_ledger.tsv", "tampered\n");
        try {
            static_cast<void>(load_count_fact_bundle(root));
            throw std::runtime_error("tampered bundle was accepted");
        } catch (const std::runtime_error& error) {
            if (std::string(error.what()) == "tampered bundle was accepted") {
                throw;
            }
        }
        std::filesystem::remove_all(root);
        std::cout << "count_fact_bundle_smoke passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
