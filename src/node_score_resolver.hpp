#pragma once

#include "reference_assignment_projection.hpp"
#include <vg/vg.pb.h>

#include <atomic>

namespace pancollapse {

struct NodeScoreStats {
    // Represented complete routes, saturated at UINT64_MAX; not DFS work.
    std::uint64_t complete_alignments = 0, subpath_visits = 0, model_visits = 0;
    std::uint64_t dp_states = 0, dp_transitions = 0, max_frontier_states = 0;
    std::uint64_t raw_models = 0, model_groups = 0, read_offset_conflicts = 0;
    // model_visits counts actual class-by-piece transfers. The weighted raw
    // count shows how much model-by-piece construction the early quotient saved.
    std::uint64_t membership_groups = 0, raw_model_transfers = 0;
    std::uint64_t frontier_compactions = 0;
    std::uint64_t balance_saturated_groups = 0, balance_unsaturated_groups = 0;
};

// Explicit test controls, never a production fallback or a counting-policy flag.
enum class NodeScoreExecution { automatic, force_map, force_sparse, enumerate_oracle };

// Exact max-DAG scoring over the alignments encoded by GAMP. Annotation lookups
// use node IDs; DP summaries contain no ordered reference walk or coordinates.
class NodeScoreResolver {
  public:
    explicit NodeScoreResolver(const count_reference::Source& reference,
                               NodeScoreExecution execution = NodeScoreExecution::automatic,
                               std::size_t frontier_batch = 64)
        : reference_(reference), execution_(execution), frontier_batch_(frontier_batch) {}
    std::vector<NumericResolvedAssignment> resolve(
        const std::vector<const vg::MultipathAlignment*>& records,
        std::span<const NumericAssignmentProfile> profiles) const;
    NodeScoreStats stats() const noexcept {
        return {complete_alignments_.load(std::memory_order_relaxed),
                subpath_visits_.load(std::memory_order_relaxed),
                model_visits_.load(std::memory_order_relaxed),
                dp_states_.load(std::memory_order_relaxed),
                dp_transitions_.load(std::memory_order_relaxed),
                max_frontier_states_.load(std::memory_order_relaxed),
                raw_models_.load(std::memory_order_relaxed),
                model_groups_.load(std::memory_order_relaxed),
                read_offset_conflicts_.load(std::memory_order_relaxed),
                membership_groups_.load(std::memory_order_relaxed),
                raw_model_transfers_.load(std::memory_order_relaxed),
                frontier_compactions_.load(std::memory_order_relaxed),
                balance_saturated_groups_.load(std::memory_order_relaxed),
                balance_unsaturated_groups_.load(std::memory_order_relaxed)};
    }

  private:
    const count_reference::Source& reference_;
    NodeScoreExecution execution_;
    std::size_t frontier_batch_ = 64;
    mutable std::atomic<std::uint64_t> complete_alignments_{0}, subpath_visits_{0}, model_visits_{0};
    mutable std::atomic<std::uint64_t> dp_states_{0}, dp_transitions_{0}, max_frontier_states_{0};
    mutable std::atomic<std::uint64_t> raw_models_{0}, model_groups_{0}, read_offset_conflicts_{0};
    mutable std::atomic<std::uint64_t> membership_groups_{0}, raw_model_transfers_{0};
    mutable std::atomic<std::uint64_t> frontier_compactions_{0};
    mutable std::atomic<std::uint64_t> balance_saturated_groups_{0}, balance_unsaturated_groups_{0};
};

}  // namespace pancollapse
