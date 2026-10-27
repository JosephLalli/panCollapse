#include "direct_count_diagnostics.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace pancollapse::direct_count;

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}  // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "pancollapse-direct-diagnostics-smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    try {
        CountRuntimeOptions runtime_options;
        runtime_options.spill_directory = root / "spill";
        runtime_options.raw_barcode_length = 4;
        runtime_options.raw_umi_length = 4;
        CountRuntime runtime({effective_profile(ProfileId::cr7_v1),
                              effective_profile(ProfileId::pansc_strict_v1)},
                             {"AAAA", "AAAT"}, {"G"}, runtime_options);
        auto worker = runtime.make_worker();
        for (int index = 0; index < 10; ++index) worker.observe_barcode("AAAA");
        worker.flush();
        runtime.finalize();

        const auto write_spool = [&](const std::filesystem::path& path,
                                     const std::vector<DirectCountDiagnosticRecord>& records) {
            DirectCountDiagnosticsSpool spool(path);
            for (const auto& record : records) spool.append(record);
            spool.finish();
        };
        const std::vector<DirectCountDiagnosticRecord> all_records{
            {7, "read_b", 0, std::string("AAAG"), std::string("FFFF"),
             std::string("ACGT"), "assigned_unique", std::string("G"),
             std::string("E/F"), 9},
            {7, "read_b", 1, std::string("AAAG"), std::string("FFFF"),
             std::string("ACGT"), "assigned_unique", std::string("G"),
             std::string("E/F"), 9},
            {8, "read_c", 0, std::nullopt, std::nullopt, std::nullopt,
             "unassigned_no_evidence", std::nullopt, std::nullopt, 1},
        };
        const auto spool_path = root / "diagnostics.spool.zst";
        write_spool(spool_path, all_records);
        const auto worker_a = root / "worker-a.spool.zst";
        const auto worker_b = root / "worker-b.spool.zst";
        const auto worker_empty = root / "worker-empty.spool.zst";
        write_spool(worker_a, {all_records[0], all_records[2]});
        write_spool(worker_b, {all_records[1]});
        write_spool(worker_empty, {});

        DirectCountDiagnosticsParquetOptions options;
        options.output_path = root / "diagnostics.parquet";
        options.row_group_rows = 1;
        const auto result = write_direct_count_diagnostics_parquet(spool_path, runtime, options);
        check(result.rows == 3 && result.canonical_logical_sha256.size() == 64 &&
                  result.parquet_byte_sha256.size() == 64 &&
                  std::filesystem::is_regular_file(options.output_path),
              "diagnostic Parquet receipt");

        options.output_path = root / "merged.parquet";
        const auto merged = write_direct_count_diagnostics_parquet(
            std::vector<std::filesystem::path>{worker_a, worker_b, worker_empty}, runtime,
            options);
        check(merged.rows == result.rows &&
                  merged.canonical_logical_sha256 == result.canonical_logical_sha256 &&
                  merged.parquet_byte_sha256 == result.parquet_byte_sha256,
              "merged diagnostic spool receipt must match single spool");

        const auto ordered_a = root / "ordered-a.spool.zst";
        const auto ordered_b = root / "ordered-b.spool.zst";
        write_spool(ordered_a, {{0, "first", 0, std::nullopt, std::nullopt, std::nullopt,
                                "unassigned_no_evidence", std::nullopt, std::nullopt, 0},
                               {1, "second", 0, std::nullopt, std::nullopt, std::nullopt,
                                "unassigned_no_evidence", std::nullopt, std::nullopt, 0}});
        write_spool(ordered_b, {{0, "first", 1, std::nullopt, std::nullopt, std::nullopt,
                                "unassigned_no_evidence", std::nullopt, std::nullopt, 0},
                               {1, "second", 1, std::nullopt, std::nullopt, std::nullopt,
                                "unassigned_no_evidence", std::nullopt, std::nullopt, 0}});
        options.output_path = root / "ordered.parquet";
        options.expected_read_groups = 2;
        const auto ordered = write_direct_count_diagnostics_parquet(
            std::vector<std::filesystem::path>{ordered_a, ordered_b}, runtime, options);
        check(ordered.rows == 4, "expected diagnostic read/profile sequence");
        options.expected_read_groups.reset();

        size_t rejection_index = 0;
        const auto rejected = [&](const std::vector<std::filesystem::path>& paths) {
            options.output_path = root / ("rejected-" + std::to_string(rejection_index++) + ".parquet");
            bool did_reject = false;
            try { write_direct_count_diagnostics_parquet(paths, runtime, options); }
            catch (const std::runtime_error&) { did_reject = true; }
            check(did_reject, "invalid diagnostics spool merge must be rejected");
        };

        const auto truncated = root / "truncated.spool.zst";
        std::filesystem::copy_file(worker_b, truncated);
        std::filesystem::resize_file(truncated, std::filesystem::file_size(truncated) - 1);
        rejected({worker_a, truncated});
        const auto trailing = root / "trailing.spool.zst";
        std::filesystem::copy_file(worker_b, trailing);
        std::ofstream(trailing, std::ios::binary | std::ios::app).put('x');
        rejected({worker_a, trailing});
        const auto duplicate = root / "duplicate.spool.zst";
        write_spool(duplicate, {all_records[0]});
        rejected({worker_a, duplicate});
        const auto descending = root / "descending.spool.zst";
        write_spool(descending, {{9, "later", 1, std::nullopt, std::nullopt, std::nullopt,
                                  "unassigned_no_evidence", std::nullopt, std::nullopt, 0},
                                 {9, "earlier-profile", 0, std::nullopt, std::nullopt,
                                  std::nullopt, "unassigned_no_evidence", std::nullopt,
                                  std::nullopt, 0}});
        rejected({worker_a, descending});
        const auto invalid_profile = root / "invalid-profile.spool.zst";
        write_spool(invalid_profile, {{10, "invalid", 2, std::nullopt, std::nullopt,
                                       std::nullopt, "unassigned_no_evidence", std::nullopt,
                                       std::nullopt, 0}});
        rejected({worker_a, invalid_profile});

        std::vector<std::filesystem::path> thirty_three;
        for (size_t index = 0; index < 33; ++index) {
            const auto path = root / ("fanin-" + std::to_string(index) + ".spool.zst");
            write_spool(path, {{static_cast<std::uint64_t>(20 + index), "fanin", 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                "unassigned_no_evidence", std::nullopt, std::nullopt, 0}});
            thirty_three.push_back(path);
        }
        options.output_path = root / "fanin.parquet";
        const auto fanin = write_direct_count_diagnostics_parquet(thirty_three, runtime, options);
        check(fanin.rows == 33, "33-way diagnostics merge must use bounded fan-in");
        std::filesystem::remove_all(root);
        std::cout << "direct_count_diagnostics smoke: ok\n";
    } catch (...) {
        std::filesystem::remove_all(root);
        throw;
    }
}
