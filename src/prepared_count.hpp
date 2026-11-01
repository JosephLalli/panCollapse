#pragma once

#include "direct_count.hpp"
#include "gamp_alignment_complexity.hpp"
#include "count_memory_budget.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace pancollapse::prepared_count {

struct Config {
    std::filesystem::path index;
    std::filesystem::path gamp;
    std::filesystem::path whitelist;
    std::filesystem::path outdir;
    std::vector<direct_count::EffectiveProfile> profiles;
    std::size_t threads = 1;
    std::size_t cb_length = 16;
    std::size_t umi_length = 12;
    std::uint64_t max_alignments_per_read = input::kDefaultMaxAlignmentsPerRead;
    bool write_10x_mex = false;
    bool preload_only = false;
    bool fail_molecule_identity = false;
    std::string version;
    std::string command_line;
    std::string analysis_scope = "frozen-profiles";
    count_memory::Request memory;
};

// Runs the prepared node-count route after the caller has validated CLI policy.
// Resolver assignments are passed unchanged to CountRuntime.
int run(const Config& config);

}  // namespace pancollapse::prepared_count
