#pragma once

#include "direct_count.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pancollapse::direct_count {

struct CountRuntimeOptions {
    size_t raw_barcode_length = 16;
    size_t raw_umi_length = 12;
    // Certified lexical dictionary from the native index: use it directly,
    // without sorting or rebuilding a reference-wide string hash table.
    bool feature_catalog_prepared = false;
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
    // Retained in the public result schema for compatibility. The memory-only
    // runtime never writes aggregate runs, so these always remain zero.
    std::uint64_t aggregate_spill_runs = 0;
    std::uint64_t aggregate_spill_rows = 0;
    std::uint64_t aggregate_spill_bytes = 0;
};

// The barcode prior, profile-zero-owned barcode-correction counters, and legacy
// compatibility counters are pass-global. Every profile-sensitive counter below
// is retained separately so a joint pass can be compared exactly with
// single-profile runs.
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
    // Native assignment path: singleton feature IDs are already resolved by
    // the prepared count index and never require feature-name materialization.
    void observe_assignment_ids(std::uint32_t profile_index,
                                std::string_view raw_barcode,
                                std::optional<std::string_view> barcode_quality,
                                std::string_view umi,
                                std::span<const std::uint32_t> gene_ids,
                                std::uint64_t read_count = 1,
                                bool reaches_umi_filter = true);
    // This is deliberately counter-free: it is only a CB/UMI prefilter, not
    // barcode posterior correction (which depends on EOF priors).
    bool potentially_countable(std::string_view raw_barcode,
                               std::string_view umi) const;
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
        std::uint8_t disposition, std::optional<std::uint32_t> feature_index,
        std::uint64_t read_count);
    std::optional<CountWorker::Pending> prepare_string_assignment(
        std::uint32_t profile_index, std::string_view raw_barcode,
        std::optional<std::string_view> barcode_quality, std::string_view umi,
        const std::vector<std::string>& genes, std::uint64_t read_count,
        bool reaches_umi_filter);
    std::optional<CountWorker::Pending> prepare_id_assignment(
        std::uint32_t profile_index, std::string_view raw_barcode,
        std::optional<std::string_view> barcode_quality, std::string_view umi,
        std::span<const std::uint32_t> gene_ids, std::uint64_t read_count,
        bool reaches_umi_filter);
    bool potentially_countable(std::string_view raw_barcode,
                               std::string_view umi) const;
    void merge_worker_batch(std::vector<CountWorker::Pending>& pending);
    std::unique_ptr<Impl> impl_;
};

std::vector<std::string> read_barcode_whitelist(const std::filesystem::path& path,
                                                size_t expected_length);

}  // namespace pancollapse::direct_count
