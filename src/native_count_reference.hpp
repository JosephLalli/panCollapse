#pragma once

#include "count_reference.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace pancollapse {

struct NativeEvidenceCacheStats {
    std::uint64_t capacity_bytes = 0;
    std::uint64_t resident_bytes = 0;
    std::uint64_t high_water_bytes = 0;
    std::uint64_t maximum_entry_bytes = 0;
    std::uint64_t entries = 0;
    std::uint64_t builds = 0;
    std::uint64_t hits = 0;
    std::uint64_t evictions = 0;
    std::uint64_t uncached = 0;
};

enum class NativeQueryMode {
    full,
    node_score,
};

struct NativeQueryStats {
    std::uint64_t native_queries = 0;
    std::uint64_t native_occurrences = 0;
    std::uint64_t native_max_occurrences = 0;
    std::uint64_t node_queries = 0;
    std::uint64_t edge_queries = 0;
    std::uint64_t edge_model_builds = 0;
    std::uint64_t max_node_occurrences = 0;
    std::uint64_t max_node_models = 0;
    std::uint64_t splice_probe_common_empty = 0;
    std::uint64_t splice_probe_attempts = 0;
    std::uint64_t splice_probe_sa_steps = 0;
    std::uint64_t splice_probe_success = 0;
    std::uint64_t splice_probe_budget_fallback = 0;
    std::uint64_t splice_probe_exhausted_without_proof = 0;
    std::uint64_t splice_adjacency_checks = 0, splice_adjacency_success = 0;
    NativeEvidenceCacheStats node_cache;
    NativeEvidenceCacheStats edge_cache;
    NativeEvidenceCacheStats splice_cache;
    // Conservative sum of complete value charges for every worker lease:
    // shared values can be counted more than once. Not a process RSS bound.
    std::uint64_t active_lease_bytes = 0;
    std::uint64_t active_lease_high_water_bytes = 0;
};

// Read evidence comes from the current GAMP group's nodes and local transitions
// only. The native annotation object supplies the ready path/pair facts; XG,
// GBWT, and its prebuilt FastLocate index are attached without path preparation.
class NativeSource final : public count_reference::Source {
  public:
    static std::unique_ptr<NativeSource> load(const std::filesystem::path& index,
                                              const std::filesystem::path& xg,
                                              const std::filesystem::path& gbwt,
                                              const std::filesystem::path& r_index,
                                              std::size_t threads,
                                              NativeQueryMode mode = NativeQueryMode::full,
                                              std::size_t max_probe_sa_steps = 4096,
                                              bool enable_adjacency_certificate = true);
    ~NativeSource() override;

    NativeSource(const NativeSource&) = delete;
    NativeSource& operator=(const NativeSource&) = delete;
    NativeSource(NativeSource&&) = delete;
    NativeSource& operator=(NativeSource&&) = delete;

    const std::vector<std::string>& strings() const override;
    const std::vector<std::vector<std::uint32_t>>& sets() const override;
    std::span<const count_reference::GeneRecord> genes() const override;
    std::span<const count_reference::PolicyRecord> policies() const override;
    const std::string& content_id() const override;
    std::size_t model_count() const override;
    count_reference::ModelRecord model(std::uint32_t id) const override;
    std::size_t scoring_class_count() const override;
    count_reference::ScoringClassRecord scoring_class(std::uint32_t id) const override;
    const count_reference::NodeRecord* node(std::uint64_t id) const override;
    std::span<const count_reference::Posting>
    node_postings(const count_reference::NodeRecord& record) const override;
    std::span<const count_reference::ScoringPosting>
    node_scoring(const count_reference::NodeRecord& record) const override;
    std::span<const count_reference::ScoringBinding>
    scoring_bindings(const count_reference::ScoringClassRecord& record) const override;
    std::span<const count_reference::EdgePosting>
    edge(std::uint64_t from, std::uint64_t to) const override;
    std::span<const std::uint32_t>
    splice_targets(std::uint64_t physical_a, std::uint64_t physical_b) const override;
    std::span<const count_reference::JunctionOwner>
    junction_owners(std::uint64_t from, std::uint64_t to) const override;
    void begin_read_group() const override;

    // The native metadata object already carries the certified output feature
    // catalog. Moving it out is not a reference query or a rebuilding step.
    std::vector<std::string> take_feature_catalog();
    // Called once after loading and before any workers or reference queries.
    void configure_cache_limits(std::size_t node_bytes, std::size_t edge_bytes,
                                std::size_t splice_bytes);
    NativeQueryStats stats() const noexcept;

  private:
    struct Impl;
    explicit NativeSource(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace pancollapse
