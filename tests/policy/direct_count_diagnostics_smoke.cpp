#include "direct_count_diagnostics.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

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
        CountRuntime runtime({effective_profile(ProfileId::pansc_strict_v1)},
                             {"AAAA", "AAAT"}, {"G"}, runtime_options);
        auto worker = runtime.make_worker();
        for (int index = 0; index < 10; ++index) worker.observe_barcode("AAAA");
        worker.flush();
        runtime.finalize();

        const auto spool_path = root / "diagnostics.spool.zst";
        DirectCountDiagnosticsSpool spool(spool_path);
        spool.append({7, "read_b", 0, std::string("AAAG"), std::string("FFFF"),
                      std::string("ACGT"), "assigned_unique", std::string("G"),
                      std::string("E/F"), 9});
        spool.append({8, "read_c", 0, std::nullopt, std::nullopt, std::nullopt,
                      "unassigned_no_evidence", std::nullopt, std::nullopt, 1});
        spool.finish();

        DirectCountDiagnosticsParquetOptions options;
        options.output_path = root / "diagnostics.parquet";
        options.row_group_rows = 1;
        const auto result = write_direct_count_diagnostics_parquet(spool_path, runtime, options);
        check(result.rows == 2 && result.canonical_logical_sha256.size() == 64 &&
                  result.parquet_byte_sha256.size() == 64 &&
                  std::filesystem::is_regular_file(options.output_path),
              "diagnostic Parquet receipt");

        const auto truncated = root / "truncated.spool.zst";
        std::filesystem::copy_file(spool_path, truncated);
        std::filesystem::resize_file(truncated, std::filesystem::file_size(truncated) - 1);
        bool rejected = false;
        try { write_direct_count_diagnostics_parquet(truncated, runtime, options); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "truncated diagnostic spool must be rejected");
        std::filesystem::remove_all(root);
        std::cout << "direct_count_diagnostics smoke: ok\n";
    } catch (...) {
        std::filesystem::remove_all(root);
        throw;
    }
}
