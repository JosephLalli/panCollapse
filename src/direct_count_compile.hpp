#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace pancollapse::direct_index {

// Builds a read-ready direct-facts index from the immutable native annotation
// binding and graph indexes. This is an offline operation; it never serves
// GAMP reads or mutates its inputs.
struct BuildOptions {
    std::filesystem::path source_index;
    std::filesystem::path xg;
    std::filesystem::path gbwt;
    std::filesystem::path r_index;
    std::filesystem::path output;
    std::size_t threads = 1;
    // Overall process-memory target. The compiler subtracts post-load resident
    // input memory, the full still-faultable native mapping, and a 10% safety
    // margin before partitioning Writer, candidate, scratch, and batch state.
    // Immutable inputs are never evicted or partially read to meet this limit;
    // the external container/cgroup remains the hard enforcement boundary.
    std::size_t memory_limit_bytes = std::size_t{1} << 30;
    // Test-only deterministic pressure control. A nonzero value may only lower
    // the retained-payload allowance derived from memory_limit_bytes.
    std::size_t testing_node_payload_limit_bytes = 0;
    std::string provenance_json;
};

void build(const BuildOptions& options);

}  // namespace pancollapse::direct_index
