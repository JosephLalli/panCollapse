#pragma once

#include "direct_count_runtime.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace pancollapse::direct_count {

// This is intentionally a post-count audit surface.  It never participates in
// barcode priors, assignment, UMI collapse, or matrix emission.
struct DirectCountDiagnosticRecord {
    std::uint64_t ordinal = 0;
    std::string qname;
    std::uint32_t profile_index = 0;
    std::optional<std::string> raw_barcode;
    std::optional<std::string> raw_barcode_quality;
    std::optional<std::string> umi;
    std::string terminal_class;
    std::optional<std::string> selected_gene;
    std::optional<std::string> selected_tier;
    std::uint64_t reason_bits = 0;
    bool barcode_correction_eligible = false;
};

// The .zst stream has an internal magic/version header and terminal count/hash
// record; a missing or partial trailer is a hard error, not an empty diagnostic.
class DirectCountDiagnosticsSpool {
  public:
    explicit DirectCountDiagnosticsSpool(const std::filesystem::path& path);
    DirectCountDiagnosticsSpool(const DirectCountDiagnosticsSpool&) = delete;
    DirectCountDiagnosticsSpool& operator=(const DirectCountDiagnosticsSpool&) = delete;
    DirectCountDiagnosticsSpool(DirectCountDiagnosticsSpool&&) noexcept;
    DirectCountDiagnosticsSpool& operator=(DirectCountDiagnosticsSpool&&) noexcept;
    ~DirectCountDiagnosticsSpool();

    void append(const DirectCountDiagnosticRecord& record);
    void finish();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct DirectCountDiagnosticsParquetOptions {
    std::filesystem::path output_path;
    std::uint64_t row_group_rows = 65536;
};
struct DirectCountDiagnosticsParquetResult {
    std::uint64_t rows = 0;
    std::string canonical_logical_sha256;
    std::string parquet_byte_sha256;
};

// Requires CountRuntime's finalized correction lookup.  The runtime owns the
// posterior/prior state; this function only turns its barcode index into the
// runtime whitelist spelling.
DirectCountDiagnosticsParquetResult write_direct_count_diagnostics_parquet(
    const std::filesystem::path& spool_path, const CountRuntime& runtime,
    const DirectCountDiagnosticsParquetOptions& options);

}  // namespace pancollapse::direct_count
