#include "direct_count_compatibility.hpp"

#include <parquet/file_reader.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace pancollapse::direct_count;

namespace {
std::string bytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int row_groups(const std::filesystem::path& path) {
    auto reader = parquet::ParquetFileReader::OpenFile(path.string(), false);
    return reader->metadata()->num_row_groups();
}
}  // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("pancollapse-compatibility-smoke-" + std::to_string(getpid()));
    std::filesystem::remove_all(root);
    try {
        CompatibilityFactSet repeated;
        repeated.exact = {
            {"TX2", "locus2", "path2", "parent2", 10, EvidenceTier::partial_exon,
             EvidenceStrand::reverse},
            {"TX1", "locus1", "path1", "parent1", 12, EvidenceTier::exon,
             EvidenceStrand::forward},
            {"TX1", "locus1", "path1", "parent1", 12, EvidenceTier::exon,
             EvidenceStrand::forward},
            {"TX3", "locus3", "path3", "parent3", 9, EvidenceTier::body,
             EvidenceStrand::forward},
        };
        repeated.structural = {
            {"TX1", StructuralLayer::unspliced, 8, EvidenceStrand::forward,
             {"path-b", "path-a", "path-a"}, {"parent-b", "parent-a", "parent-a"}},
            {"TX2", StructuralLayer::spliced, 7, EvidenceStrand::reverse,
             {"path-c"}, {"parent-c"}},
        };
        CompatibilityFactSet empty;
        empty.complete_provenance = false;
        CompatibilityBundle bundle;
        bundle.producer_version = "0.10.0-smoke";
        bundle.compatibility_algorithm_id = "pancollapse-ex50pas-compatibility-v1";
        bundle.structural_surface_id = "sha256:" + std::string(64, '3');
        bundle.input_records = 4;
        bundle.input_read_groups = 3;
        // Deliberately unsorted: the persisted manifest must canonicalize input roles.
        bundle.inputs = {
            {"xg", std::string(64, '2'), 200},
            {"gamp", std::string(64, '1'), 100},
        };
        bundle.fact_sets = {repeated, repeated, empty};
        bundle.reads = {
            {0, "read-valid", MoleculeStatus::valid, "ACGT", "IIII", "TGCA", "JJJJ", 1},
            {1, "read-invalid", MoleculeStatus::malformed, std::nullopt, std::nullopt,
             std::nullopt, std::nullopt, 2},
            {2, "read-featureless", MoleculeStatus::valid, "TTTT", std::nullopt,
             "AAAA", std::nullopt, 2},
        };
        CompatibilityWriteOptions options;
        options.output_directory = root / "first";
        options.parquet_row_group_rows = 2;
        options.parquet_max_string_bytes_per_batch = 40;
        const auto receipt = write_compatibility_bundle(bundle, options);
        require(receipt.tables.size() == 4 && !receipt.content_id.empty() &&
                    receipt.manifest_sha256.size() == 64,
                "writer did not emit a checksum-bound four-table bundle");
        const CompatibilityBundle replay = read_compatibility_bundle(options.output_directory);
        require(replay.reads.size() == 3 && replay.fact_sets.size() == 2,
                "round-trip did not retain rows or intern equal states");
        require(replay.reads[0].fact_set_id == 1 && replay.reads[1].fact_set_id == 0 &&
                    replay.reads[2].fact_set_id == 0,
                "fact-set interning/remapping was not deterministic");
        require(replay.reads[1].molecule_status == MoleculeStatus::malformed &&
                    replay.reads[2].original_name == "read-featureless",
                "invalid or featureless input denominator was not retained");
        require(replay.fact_sets[1].exact.size() == 3 &&
                    replay.fact_sets[1].structural[0].winning_paths ==
                        std::vector<std::string>({"path-a", "path-b"}),
                "fact canonicalization/deduplication failed");
        require(row_groups(root / "first/parquet/read_rows.parquet") == 3 &&
                    row_groups(root / "first/parquet/exact_facts.parquet") == 3 &&
                    row_groups(root / "first/parquet/structural_facts.parquet") == 2,
                "compatibility string budget did not force bounded row groups");
        require(replay.inputs.size() == 2 && replay.inputs[0].role == "gamp" &&
                    replay.producer_version == bundle.producer_version &&
                    replay.compatibility_algorithm_id == bundle.compatibility_algorithm_id &&
                    replay.structural_surface_id == bundle.structural_surface_id &&
                    replay.input_records == bundle.input_records &&
                    replay.input_read_groups == bundle.input_read_groups &&
                    replay.molecule_status_counts ==
                        MoleculeStatusCounts{2, 0, 1, 0} &&
                    replay.content_id == receipt.content_id,
                "manifest identity did not round-trip");

        CompatibilityBundle featureless = bundle;
        featureless.input_records = 2;
        featureless.input_read_groups = 2;
        featureless.fact_sets = {empty};
        featureless.reads = {
            {0, "empty-valid", MoleculeStatus::valid, "ACGT", std::nullopt,
             "TGCA", std::nullopt, 0},
            {1, "empty-missing", MoleculeStatus::missing, std::nullopt,
             std::nullopt, std::nullopt, std::nullopt, 0},
        };
        options.output_directory = root / "featureless";
        const auto featureless_receipt =
            write_compatibility_bundle(featureless, options);
        const CompatibilityBundle featureless_replay =
            read_compatibility_bundle(options.output_directory);
        require(featureless_receipt.tables.size() == 4 &&
                    featureless_replay.reads.size() == 2 &&
                    featureless_replay.fact_sets.size() == 1 &&
                    featureless_replay.fact_sets[0].exact.empty() &&
                    featureless_replay.fact_sets[0].structural.empty() &&
                    row_groups(root / "featureless/parquet/exact_facts.parquet") == 0 &&
                    row_groups(root / "featureless/parquet/structural_facts.parquet") == 0,
                "zero-row fact tables did not publish and replay cleanly");

        CompatibilityBundle oversized_fact = bundle;
        oversized_fact.fact_sets[1].exact.front().path = std::string(41, 'x');
        options.output_directory = root / "finalize-failure";
        bool finalize_rejected = false;
        try { (void)write_compatibility_bundle(std::move(oversized_fact), options); }
        catch (const std::exception&) { finalize_rejected = true; }
        require(finalize_rejected &&
                    !std::filesystem::exists(options.output_directory) &&
                    !std::filesystem::exists(
                        root / (".finalize-failure.compatibility-staging-" +
                                std::to_string(getpid()))),
                "failed fact-table finalization left a destination or staging tree");

        options.parquet_row_group_rows = 100;
        options.parquet_max_string_bytes_per_batch = 1U << 20;
        options.parquet_max_list_values_per_batch = 2;
        options.output_directory = root / "list-bounded";
        const auto list_bounded_receipt =
            write_compatibility_bundle(bundle, options);
        require(list_bounded_receipt.tables.size() == 4 &&
                    row_groups(root / "list-bounded/parquet/structural_facts.parquet") == 2,
                "compatibility list-value budget did not force bounded row groups");

        options.output_directory = root / "list-finalize-failure";
        options.parquet_max_list_values_per_batch = 1;
        finalize_rejected = false;
        try { (void)write_compatibility_bundle(bundle, options); }
        catch (const std::exception&) { finalize_rejected = true; }
        require(finalize_rejected &&
                    !std::filesystem::exists(options.output_directory) &&
                    !std::filesystem::exists(
                        root / (".list-finalize-failure.compatibility-staging-" +
                                std::to_string(getpid()))),
                "failed list-bounded finalization left a destination or staging tree");

        options.parquet_max_list_values_per_batch =
            kMaximumCompatibilityListValuesPerBatch + 1;
        options.output_directory = root / "oversized-list-batch";
        bool list_rejected = false;
        try { (void)write_compatibility_bundle(bundle, options); }
        catch (const std::exception&) { list_rejected = true; }
        require(list_rejected && !std::filesystem::exists(options.output_directory),
                "writer accepted an unbounded Parquet list-value batch");
        options.parquet_max_list_values_per_batch =
            kMaximumCompatibilityListValuesPerBatch;
        options.parquet_row_group_rows = 2;
        options.parquet_max_string_bytes_per_batch = 40;

        CompatibilityBundle wrong_denominator = bundle;
        wrong_denominator.input_read_groups = 4;
        options.output_directory = root / "wrong-denominator";
        bool denominator_rejected = false;
        try {
            static_cast<void>(write_compatibility_bundle(
                std::move(wrong_denominator), options));
        } catch (const std::exception&) {
            denominator_rejected = true;
        }
        require(denominator_rejected &&
                    !std::filesystem::exists(options.output_directory),
                "compatibility writer accepted a dropped read-group denominator");

        options.output_directory = root / "second";
        CompatibilityBundleWriter streaming_writer(
            {bundle.inputs, bundle.producer_version,
             bundle.compatibility_algorithm_id, bundle.structural_surface_id,
             bundle.input_records, bundle.input_read_groups},
            options);
        for (const ReadCompatibilityRow& read : bundle.reads) {
            streaming_writer.append(read, bundle.fact_sets.at(read.fact_set_id));
        }
        const CompatibilityWriteReceipt streaming_receipt =
            streaming_writer.finalize();
        require(streaming_receipt.content_id == receipt.content_id &&
                    streaming_writer.rows_appended() == bundle.reads.size(),
                "streaming and convenience writers produced different identities");
        for (const auto& relative : {"manifest.json", "parquet/read_rows.parquet",
                                     "parquet/fact_sets.parquet", "parquet/exact_facts.parquet",
                                     "parquet/structural_facts.parquet"}) {
            require(bytes(root / "first" / relative) == bytes(root / "second" / relative),
                    "batch and streaming writes are not byte-identical");
        }
        CompatibilityBundleReader streaming_reader(root / "second");
        std::vector<std::string> streamed_names;
        size_t streamed_batches = 0;
        streaming_reader.for_each_read_batch(
            [&](std::span<const ReadCompatibilityRow> rows) {
                require(rows.size() == 1,
                        "streaming reader exceeded its requested batch bound");
                ++streamed_batches;
                streamed_names.push_back(rows.front().original_name);
            },
            1);
        require(streamed_batches == 3 &&
                    streamed_names == std::vector<std::string>(
                        {"read-valid", "read-invalid", "read-featureless"}) &&
                    streaming_reader.index().fact_sets.size() == 2,
                "streaming reader lost order, rows, or interned facts");
        bool oversized_batch_rejected = false;
        try {
            streaming_reader.for_each_read_batch(
                [](std::span<const ReadCompatibilityRow>) {},
                kMaximumCompatibilityRowsPerBatch + 1);
        } catch (const std::exception&) {
            oversized_batch_rejected = true;
        }
        require(oversized_batch_rejected,
                "streaming reader accepted an unbounded batch request");
        {
            std::fstream corrupt(root / "second/parquet/read_rows.parquet",
                                 std::ios::in | std::ios::out | std::ios::binary);
            corrupt.seekp(8);
            corrupt.put('X');
        }
        bool rejected = false;
        try { (void)read_compatibility_bundle(root / "second"); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "corrupted compatibility table was accepted");

        options.output_directory = root / "third";
        write_compatibility_bundle(bundle, options);
        {
            const auto manifest_path = root / "third/manifest.json";
            std::string manifest = bytes(manifest_path);
            const size_t at = manifest.find("\"logical_sha256\":\"");
            require(at != std::string::npos, "manifest lacks logical hash");
            const size_t digit = at + std::string("\"logical_sha256\":\"").size();
            manifest[digit] = manifest[digit] == '0' ? '1' : '0';
            std::ofstream output(manifest_path, std::ios::binary | std::ios::trunc);
            output << manifest;
        }
        rejected = false;
        try { (void)read_compatibility_bundle(root / "third"); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "tampered logical hash/content identity was accepted");

        options.output_directory = root / "oversized-row-group";
        options.parquet_row_group_rows = kMaximumCompatibilityRowsPerBatch + 1;
        rejected = false;
        try { (void)write_compatibility_bundle(bundle, options); }
        catch (const std::exception&) { rejected = true; }
        require(rejected && !std::filesystem::exists(options.output_directory),
                "writer accepted an unbounded Parquet row-group request");
        options.parquet_row_group_rows = 2;

        options.output_directory = root / "oversized-string-batch";
        options.parquet_max_string_bytes_per_batch =
            kMaximumCompatibilityStringBytesPerBatch + 1;
        rejected = false;
        try { (void)write_compatibility_bundle(bundle, options); }
        catch (const std::exception&) { rejected = true; }
        require(rejected && !std::filesystem::exists(options.output_directory),
                "writer accepted an unbounded Parquet string batch");
        options.parquet_max_string_bytes_per_batch = 40;

        options.output_directory = root / "abandoned";
        {
            CompatibilityBundleWriter abandoned(
                {bundle.inputs, bundle.producer_version,
                 bundle.compatibility_algorithm_id, bundle.structural_surface_id,
                 bundle.input_records, bundle.input_read_groups},
                options);
            abandoned.append(bundle.reads[0],
                             bundle.fact_sets.at(bundle.reads[0].fact_set_id));
        }
        require(!std::filesystem::exists(options.output_directory) &&
                    !std::filesystem::exists(
                        root / (".abandoned.compatibility-staging-" +
                                std::to_string(getpid()))),
                "abandoned streaming writer left a destination or spool behind");
        std::filesystem::remove_all(root);
        return 0;
    } catch (...) {
        std::filesystem::remove_all(root);
        throw;
    }
}
