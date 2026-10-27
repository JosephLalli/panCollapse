#pragma once

#include "direct_count.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pancollapse::direct_count {

struct CountRuntimeOptions {
    std::uint64_t memory_budget_bytes = 128ULL << 30;
    std::filesystem::path spill_directory;
    size_t raw_barcode_length = 16;
    size_t raw_umi_length = 12;
};

struct CountRuntimeCounters {
    std::uint64_t barcode_prior_groups = 0;
    std::uint64_t exact_barcode_prior_groups = 0;
    std::uint64_t assigned_observations = 0;
    std::uint64_t direct_multigene_dropped = 0;
    std::uint64_t invalid_umi_dropped = 0;
    std::uint64_t umi_n_dropped = 0;
    std::uint64_t umi_homopolymer_dropped = 0;
    std::uint64_t off_whitelist_uncorrectable_dropped = 0;
    std::uint64_t barcode_correction_succeeded = 0;
    std::uint64_t barcode_correction_failed = 0;
    std::uint64_t aggregate_spill_runs = 0;
    std::uint64_t aggregate_spill_rows = 0;
    std::uint64_t aggregate_spill_bytes = 0;
};

// The barcode prior, profile-zero-owned barcode-correction counters, and spill
// counters are pass-global. Every profile-sensitive counter below is retained
// separately so a joint pass can be compared exactly with single-profile runs.
struct ProfileCountRuntimeCounters {
    std::uint64_t assigned_observations = 0;
    std::uint64_t direct_multigene_dropped = 0;
    std::uint64_t invalid_umi_dropped = 0;
    std::uint64_t umi_n_dropped = 0;
    std::uint64_t umi_homopolymer_dropped = 0;
    std::uint64_t off_whitelist_uncorrectable_dropped = 0;
    std::uint64_t barcode_correction_succeeded = 0;
    std::uint64_t barcode_correction_failed = 0;

    bool operator==(const ProfileCountRuntimeCounters&) const = default;
};

struct NumericMoleculeRecord {
    std::uint32_t profile_index = 0;
    std::uint32_t barcode_whitelist_index = 0;
    std::uint32_t feature_catalog_index = 0;
    std::string corrected_umi;
    std::uint64_t supporting_reads = 0;
    std::uint64_t raw_umis_collapsed = 0;
};

struct NumericCountRecord {
    std::uint32_t profile_index = 0;
    std::uint32_t barcode_whitelist_index = 0;
    std::uint32_t feature_catalog_index = 0;
    std::uint64_t umi_count = 0;
};

struct CountRuntimeResult {
    std::vector<NumericMoleculeRecord> molecules;
    std::vector<NumericCountRecord> counts;
    std::vector<std::uint64_t> exact_barcode_priors;
    CountRuntimeCounters counters;
    std::vector<ProfileCountRuntimeCounters> profile_counters;
};

class CountRuntime;

// Workers retain a small local aggregate batch and merge it into fixed shards.
// Moving or destroying a worker flushes its batch; callers should still call
// flush() explicitly so errors are observed at the worker boundary.
class CountWorker {
  public:
    CountWorker() = default;
    CountWorker(CountRuntime& runtime, size_t batch_capacity);
    CountWorker(const CountWorker&) = delete;
    CountWorker& operator=(const CountWorker&) = delete;
    CountWorker(CountWorker&& other) noexcept;
    CountWorker& operator=(CountWorker&& other) noexcept;
    ~CountWorker();

    void observe_barcode(std::string_view raw_barcode);
    void observe_assignment(std::uint32_t profile_index,
                            std::string_view raw_barcode,
                            std::optional<std::string_view> barcode_quality,
                            std::string_view umi,
                            const std::vector<std::string>& genes,
                            std::uint64_t read_count = 1,
                            bool reaches_umi_filter = true);
    void flush();

  private:
    friend class CountRuntime;
    struct Pending;
    CountRuntime* runtime_ = nullptr;
    size_t batch_capacity_ = 0;
    std::vector<Pending> pending_;
};

class CountRuntime {
  public:
    CountRuntime(std::vector<EffectiveProfile> profiles,
                 std::vector<std::string> whitelist,
                 std::vector<std::string> feature_catalog,
                 CountRuntimeOptions options);
    ~CountRuntime();
    CountRuntime(const CountRuntime&) = delete;
    CountRuntime& operator=(const CountRuntime&) = delete;

    CountWorker make_worker(size_t batch_capacity = 4096);
    CountRuntimeResult finalize();

    const std::vector<EffectiveProfile>& profiles() const;
    const std::vector<std::string>& whitelist() const;
    const std::vector<std::string>& feature_catalog() const;
    // Uses the current all-read exact prior. Call only after barcode-producing
    // workers have joined; it remains valid after finalize().
    std::optional<std::uint32_t> corrected_barcode_index(
        std::string_view raw_barcode,
        std::optional<std::string_view> barcode_quality = std::nullopt) const;

  private:
    friend class CountWorker;
    struct Impl;
    void observe_barcode(std::string_view raw_barcode);
    std::optional<CountWorker::Pending> prepare_assignment(
        std::uint32_t profile_index, std::string_view raw_barcode,
        std::optional<std::string_view> barcode_quality, std::string_view umi,
        const std::vector<std::string>& genes, std::uint64_t read_count,
        bool reaches_umi_filter);
    void merge_worker_batch(std::vector<CountWorker::Pending>& pending);
    std::unique_ptr<Impl> impl_;
};

std::vector<std::string> read_barcode_whitelist(const std::filesystem::path& path,
                                                size_t expected_length);

}  // namespace pancollapse::direct_count
