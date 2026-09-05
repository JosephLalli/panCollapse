#pragma once

#include "direct_count_facts.hpp"
#include "direct_count_runtime.hpp"

#include <cstdint>
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pancollapse::direct_count {

struct CountInputIdentity {
    std::string role;
    std::filesystem::path path;
    std::string sha256;
    std::uint64_t size_bytes = 0;
};

struct CountTableIdentity {
    std::string relative_path;
    std::uint64_t rows = 0;
    std::uint64_t size_bytes = 0;
    std::string logical_sha256;
    std::string output_sha256;
};

// Per-profile hashes use decoded barcode and gene strings rather than compact
// output-local dictionary indexes. The same profile can therefore be compared
// directly between a joint run and a separate single-profile run.
struct CountProfileLogicalIdentity {
    std::uint64_t count_rows = 0;
    std::string counts_sha256;
    std::uint64_t molecule_rows = 0;
    std::string molecules_sha256;
};

// Links a count result to the policy-neutral compatibility bundle that was
// either emitted alongside direct GAMP traversal or consumed for replay.
// The content ID is logical identity; the manifest checksum binds the exact
// on-disk receipt used by this invocation.
struct CountCompatibilityIdentity {
    std::string mode;  // "produced" or "replayed"
    std::filesystem::path manifest_path;
    std::string content_id;
    std::string manifest_sha256;
    std::uint64_t manifest_size_bytes = 0;
};

struct CountOutputOptions {
    std::filesystem::path output_directory;
    // Optional caller-owned sibling stage (for an ordered RAD/diagnostic sink
    // produced during GAMP traversal). It must exist and the final destination
    // must not; this function still performs the final atomic rename.
    std::filesystem::path staging_directory;
    std::string pancollapse_version;
    std::string command_line;
    std::string analysis_scope = "frozen-profiles";
    std::string fact_bundle_content_id;
    std::optional<CountCompatibilityIdentity> compatibility_bundle;
    std::vector<CountInputIdentity> inputs;
    std::uint64_t threads = 1;
    std::uint64_t memory_budget_bytes = 0;
    std::uint64_t assignment_cache_capacity = 0;
    std::uint64_t assignment_cache_entries = 0;
    std::uint64_t assignment_cache_hits = 0;
    std::uint64_t assignment_cache_misses = 0;
    std::uint64_t assignment_cache_uncached = 0;
    std::uint64_t input_records = 0;
    std::uint64_t input_read_groups = 0;
    std::uint64_t raw_molecule_missing_groups = 0;
    std::uint64_t raw_molecule_malformed_groups = 0;
    std::uint64_t raw_molecule_unsupported_groups = 0;
    std::uint64_t raw_molecule_skipped_groups = 0;
    std::vector<std::array<std::uint64_t, kAssignmentTerminalCount>>
        profile_assignment_terminals;
    double initialization_seconds = 0.0;
    double processing_seconds = 0.0;
    bool write_10x_mex = false;
    std::uint64_t rad_records = 0;
    std::optional<CountTableIdentity> read_assignments;
    std::uint64_t parquet_row_group_rows = 65536;
};

struct CountOutputReceipt {
    std::map<std::string, CountTableIdentity> tables;
    std::map<std::string, CountTableIdentity> additional_outputs;
    std::map<std::string, CountProfileLogicalIdentity> profile_logical_tables;
};

// Writes every requested sink into a sibling staging directory, validates the
// Parquet row counts and schemas, then atomically publishes output_directory.
// The destination must not already exist.
CountOutputReceipt write_count_outputs(
    const CountRuntime& runtime, const CountRuntimeResult& result,
    const CountFactCatalog& facts, const CountOutputOptions& options);

}  // namespace pancollapse::direct_count
