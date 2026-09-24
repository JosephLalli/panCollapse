#pragma once

// PathTally steps 2-4: attribute per-node scores to the HST paths crossing each
// node along each alignment path, select the winning HSTs (top best-path score across
// a read's alignments plus ties, D091), collapse them to unique transcript IDs, and record per-transcript
// orientation by majority of aligned bases.
//
// The graph lookup ("which HST paths cross this node, and in which orientation")
// is injected as a PathLookup so this logic is testable without a graph; the
// production path implements it with xg for_each_step_on_handle.

#include "pathtally_score.hpp"

#include <vg/vg.pb.h>

#include <absl/container/flat_hash_map.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace pathtally {

struct HstTally {
    int64_t score = 0;
    int64_t forward_bases = 0;
    int64_t reverse_bases = 0;
};

// Per-read-group tally, keyed by HST path name. A flat (open-addressing) hash map
// rather than std::map: the tally is written once per node-visit (the hot loop), so
// its cost dominated the run as red-black-tree string comparisons (memcmp). The flat
// map hashes instead of comparing, and callers reuse one instance across groups
// (clear() keeps the backing storage) so per-group node allocation disappears. Order
// does not matter: accumulation is commutative and select_targets re-derives the top
// score and re-sorts.
using TallyMap = absl::flat_hash_map<std::string, HstTally>;
// Production ledgers already assign every relevant graph path a stable numeric handle. Keeping
// that handle through the hot tally avoids hashing and copying long vg path names per read; names
// are recovered only for winning BAM provenance.
using NumericTallyMap = absl::flat_hash_map<uint64_t, HstTally>;

// For a graph node id, emit(path_name, path_is_reverse) once per HST path step on
// that node.
using PathLookup =
    std::function<void(int64_t node_id, const std::function<void(const std::string&, bool)>&)>;

// Aligned (reference- and read-consuming) bases on one mapping.
inline int64_t mapping_aligned_bases(const vg::Mapping& mapping) {
    int64_t bases = 0;
    for (const vg::Edit& edit : mapping.edit()) {
        if (edit.from_length() > 0 && edit.to_length() > 0) {
            bases += edit.from_length();
        }
    }
    return bases;
}

// Strip a trailing _H<digits> or _R<digits> haplotype suffix to get the
// transcript id. Names without that suffix are returned unchanged.
inline std::string transcript_id_of(const std::string& hst_name) {
    const size_t underscore = hst_name.rfind('_');
    if (underscore == std::string::npos || underscore + 2 >= hst_name.size()) {
        return hst_name;
    }
    const char tag = hst_name[underscore + 1];
    if (tag != 'H' && tag != 'R') {
        return hst_name;
    }
    for (size_t k = underscore + 2; k < hst_name.size(); ++k) {
        if (std::isdigit(static_cast<unsigned char>(hst_name[k])) == 0) {
            return hst_name;
        }
    }
    return hst_name.substr(0, underscore);
}

// Per-subpath node scorer: fill per_node with the vg score of each node in
// mp.subpath(subpath_index), whose read segment begins at read_offset.
using NodeScorer = std::function<void(const vg::MultipathAlignment& mp, int subpath_index,
                                      size_t read_offset, std::vector<int64_t>& per_node)>;

// Default scorer: the flat (non-quality-adjusted) reproduction of vg's scheme.
inline NodeScorer flat_scorer(const ScoreParams& params = ScoreParams{}) {
    return [params](const vg::MultipathAlignment& mp, int subpath_index, size_t read_offset,
                    std::vector<int64_t>& per_node) {
        score_subpath(mp.subpath(subpath_index), read_offset, mp.sequence().size(), false, params, &per_node);
    };
}

// D091: score each alignment path separately and keep each HST's best path. An alignment path is
// one source-to-sink route through a record's subpath graph, choosing one of the alternative
// subpaths vg lists for a stretch of the read. On a path, every HST crossing a node receives that
// node's score once per HST step, as in D048; an HST's tally is the maximum over every path of
// every record that visits it, never a sum across alternatives or records. Orientation evidence
// comes from the maximizing path; among paths tied on score, the one with more aligned bases, then
// more forward bases, wins, so the result does not depend on processing order.
//
// Paths are never enumerated. Within one record, HSTs whose visits (node, orientation, count) are
// identical are grouped exactly by partition refinement and scored once; a dynamic program over
// the topologically ordered subpaths carries, per group, the best score of a route prefix that has
// visited the group and whether the subpath can be reached without visiting it. Node-to-HST
// lookups run once per distinct node per read.
struct RouteScore {
    int64_t score = 0;
    int64_t forward_bases = 0;
    int64_t reverse_bases = 0;
};

inline constexpr int64_t kNoRouteScore = std::numeric_limits<int64_t>::min();

inline bool route_score_better(const RouteScore& a, const RouteScore& b) {
    if (a.score != b.score) return a.score > b.score;
    const int64_t a_bases = a.forward_bases + a.reverse_bases;
    const int64_t b_bases = b.forward_bases + b.reverse_bases;
    if (a_bases != b_bases) return a_bases > b_bases;
    return a.forward_bases > b.forward_bases;
}

inline int64_t route_checked_add(int64_t a, int64_t b) {
    int64_t sum;
    if (__builtin_add_overflow(a, b, &sum)) {
        throw std::overflow_error("alignment-path tally overflowed int64");
    }
    return sum;
}

inline int64_t route_checked_multiply(int64_t a, int64_t b) {
    int64_t product;
    if (__builtin_mul_overflow(a, b, &product)) {
        throw std::overflow_error("alignment-path tally overflowed int64");
    }
    return product;
}

struct RouteTallyStats {
    uint64_t records = 0;
    uint64_t linear_records = 0;  // single-route records summed directly
    uint64_t packed_records = 0;  // branching records run on packed keys
    uint64_t groups = 0;
    uint64_t max_groups = 0;
    uint64_t dp_cells = 0;
    uint64_t grouping_ns = 0;
    uint64_t dp_ns = 0;
};

// Process-wide totals across worker threads, published once per read.
struct RouteTallyTotals {
    std::atomic<uint64_t> records{0};
    std::atomic<uint64_t> linear_records{0};
    std::atomic<uint64_t> packed_records{0};
    std::atomic<uint64_t> groups{0};
    std::atomic<uint64_t> max_groups{0};
    std::atomic<uint64_t> dp_cells{0};
    std::atomic<uint64_t> grouping_ns{0};
    std::atomic<uint64_t> dp_ns{0};
};

inline RouteTallyTotals& route_tally_totals() {
    static RouteTallyTotals totals;
    return totals;
}

// Packed route keys are an exact acceleration of RouteTally; tests switch them off to check the
// unpacked program on the same inputs.
inline bool& packed_route_keys_enabled() {
    static bool enabled = true;
    return enabled;
}

// Worker-owned and reused across reads so its buffers stop allocating once warm.
template<class Key, class Hash = absl::Hash<Key>>
class RouteTally {
public:
    // ref_paths(node_id, emit) must call emit(const Key&, bool path_is_reverse) once per HST step
    // on the node. out(const Key&, const RouteScore&) runs once per HST some complete path visits.
    template<class RefPaths, class Out>
    void run(const std::vector<const vg::MultipathAlignment*>& records,
             const NodeScorer& node_scorer, RefPaths&& ref_paths, Out&& out) {
        key_ids_.clear();
        keys_.clear();
        best_.clear();
        key_group_.clear();
        key_epoch_.clear();
        key_token_epoch_.clear();
        key_forward_.clear();
        key_reverse_.clear();
        key_route_epoch_.clear();
        key_route_.clear();
        node_slots_.clear();
        slot_begin_.clear();
        slot_end_.clear();
        slot_epoch_.clear();
        slot_record_index_.clear();
        steps_.clear();
        for (const vg::MultipathAlignment* record : records) {
            run_record(*record, node_scorer, ref_paths);
        }
        for (size_t key = 0; key < keys_.size(); ++key) {
            if (best_[key].score != kNoRouteScore) out(keys_[key], best_[key]);
        }
        publish_stats();
    }

private:
    void publish_stats() {
        RouteTallyTotals& totals = route_tally_totals();
        totals.records.fetch_add(stats_.records, std::memory_order_relaxed);
        totals.linear_records.fetch_add(stats_.linear_records, std::memory_order_relaxed);
        totals.packed_records.fetch_add(stats_.packed_records, std::memory_order_relaxed);
        totals.groups.fetch_add(stats_.groups, std::memory_order_relaxed);
        totals.dp_cells.fetch_add(stats_.dp_cells, std::memory_order_relaxed);
        totals.grouping_ns.fetch_add(stats_.grouping_ns, std::memory_order_relaxed);
        totals.dp_ns.fetch_add(stats_.dp_ns, std::memory_order_relaxed);
        uint64_t seen = totals.max_groups.load(std::memory_order_relaxed);
        while (stats_.max_groups > seen &&
               !totals.max_groups.compare_exchange_weak(seen, stats_.max_groups,
                                                        std::memory_order_relaxed)) {
        }
        stats_ = {};
    }

private:
    struct Step {
        uint32_t key;
        bool path_reverse;
    };
    struct Visit {
        uint32_t slot;
        int64_t score;
        int64_t bases;
        bool read_reverse;
    };
    struct Contribution {
        uint32_t group;
        int64_t forward_steps;  // steps whose HST runs along the node's forward strand
        int64_t reverse_steps;
    };

    uint32_t intern(const Key& key) {
        const auto [found, inserted] =
            key_ids_.try_emplace(key, static_cast<uint32_t>(keys_.size()));
        if (inserted) {
            keys_.push_back(key);
            best_.push_back({kNoRouteScore, 0, 0});
            key_group_.push_back(0);
            key_epoch_.push_back(0);
            key_forward_.push_back(0);
            key_reverse_.push_back(0);
            key_token_epoch_.push_back(0);
            key_route_epoch_.push_back(0);
            key_route_.push_back({});
        }
        return found->second;
    }

    template<class RefPaths>
    uint32_t slot_of(int64_t node_id, RefPaths& ref_paths) {
        const auto [found, inserted] =
            node_slots_.try_emplace(node_id, static_cast<uint32_t>(slot_begin_.size()));
        if (inserted) {
            slot_begin_.push_back(static_cast<uint32_t>(steps_.size()));
            ref_paths(node_id, [&](const Key& key, bool path_reverse) {
                steps_.push_back({intern(key), path_reverse});
            });
            slot_end_.push_back(static_cast<uint32_t>(steps_.size()));
            slot_epoch_.push_back(0);
            slot_record_index_.push_back(0);
        }
        return found->second;
    }

    // Count each HST's steps on one node by orientation into key_forward_/key_reverse_, listing
    // the HSTs in touched_keys_.
    void count_steps(uint32_t slot) {
        ++token_epoch_;
        touched_keys_.clear();
        for (uint32_t step = slot_begin_[slot]; step < slot_end_[slot]; ++step) {
            const uint32_t key = steps_[step].key;
            if (key_token_epoch_[key] != token_epoch_) {
                key_token_epoch_[key] = token_epoch_;
                key_forward_[key] = 0;
                key_reverse_[key] = 0;
                touched_keys_.push_back(key);
            }
            ++(steps_[step].path_reverse ? key_reverse_[key] : key_forward_[key]);
        }
    }

    template<class RefPaths>
    void run_record(const vg::MultipathAlignment& alignment, const NodeScorer& node_scorer,
                    RefPaths& ref_paths) {
        const size_t count = static_cast<size_t>(alignment.subpath_size());
        if (count == 0) return;
        ++epoch_;
        ++stats_.records;

        // Score each subpath's mappings and resolve their nodes.
        const std::vector<size_t> offsets = subpath_read_offsets(alignment);
        visits_.clear();
        visit_begin_.assign(count + 1, 0);
        record_slots_.clear();
        for (size_t subpath = 0; subpath < count; ++subpath) {
            node_scorer(alignment, static_cast<int>(subpath), offsets[subpath], per_node_);
            const vg::Path& path = alignment.subpath(static_cast<int>(subpath)).path();
            for (int index = 0; index < path.mapping_size(); ++index) {
                const vg::Mapping& mapping = path.mapping(index);
                const uint32_t slot = slot_of(mapping.position().node_id(), ref_paths);
                if (slot_epoch_[slot] != epoch_) {
                    slot_epoch_[slot] = epoch_;
                    slot_record_index_[slot] = static_cast<uint32_t>(record_slots_.size());
                    record_slots_.push_back(slot);
                }
                visits_.push_back({slot, per_node_[static_cast<size_t>(index)],
                                   mapping_aligned_bases(mapping),
                                   mapping.position().is_reverse()});
            }
            visit_begin_[subpath + 1] = visits_.size();
        }

        // Subpath graph: predecessors, sources, sinks, last use of each row.
        pred_count_.assign(count + 1, 0);
        has_out_.assign(count, 0);
        out_degree_.assign(count, 0);
        next_of_.assign(count, 0);
        last_use_.resize(count);
        for (size_t subpath = 0; subpath < count; ++subpath) {
            last_use_[subpath] = subpath;
            const vg::Subpath& sub = alignment.subpath(static_cast<int>(subpath));
            auto edge = [&](uint32_t next) {
                if (next >= count) {
                    throw std::runtime_error("GAMP MultipathAlignment next index is out of range");
                }
                if (next <= subpath) {
                    throw std::runtime_error(
                        "GAMP MultipathAlignment subpaths are not in topological order");
                }
                ++pred_count_[next + 1];
                has_out_[subpath] = 1;
                ++out_degree_[subpath];
                next_of_[subpath] = next;
                last_use_[subpath] = std::max<size_t>(last_use_[subpath], next);
            };
            for (const uint32_t next : sub.next()) edge(next);
            for (const vg::Connection& connection : sub.connection()) edge(connection.next());
        }
        for (size_t subpath = 0; subpath < count; ++subpath) {
            pred_count_[subpath + 1] += pred_count_[subpath];
        }
        preds_.resize(pred_count_[count]);
        pred_fill_.assign(pred_count_.begin(), pred_count_.end() - 1);
        for (size_t subpath = 0; subpath < count; ++subpath) {
            const vg::Subpath& sub = alignment.subpath(static_cast<int>(subpath));
            for (const uint32_t next : sub.next()) preds_[pred_fill_[next]++] = subpath;
            for (const vg::Connection& connection : sub.connection()) {
                preds_[pred_fill_[connection.next()]++] = subpath;
            }
        }
        is_source_.assign(count, 0);
        if (alignment.start_size() > 0) {
            for (const uint32_t start : alignment.start()) {
                if (start >= count) {
                    throw std::runtime_error("GAMP MultipathAlignment start index is out of range");
                }
                is_source_[start] = 1;
            }
        } else {
            for (size_t subpath = 0; subpath < count; ++subpath) {
                if (pred_count_[subpath + 1] == pred_count_[subpath]) is_source_[subpath] = 1;
            }
        }

        // A record with one route (a chain from its single source) needs no grouping: each HST's
        // score on the record is the plain sum along the chain.
        if (single_route(count)) {
            ++stats_.linear_records;
            tally_single_route(count);
            return;
        }

        const auto grouping_started = std::chrono::steady_clock::now();
        // Group this record's HSTs by their exact visits, one node at a time.
        record_keys_.clear();
        for (const uint32_t slot : record_slots_) {
            for (uint32_t step = slot_begin_[slot]; step < slot_end_[slot]; ++step) {
                const uint32_t key = steps_[step].key;
                if (key_epoch_[key] != epoch_) {
                    key_epoch_[key] = epoch_;
                    key_group_[key] = 0;
                    record_keys_.push_back(key);
                }
            }
        }
        if (record_keys_.empty()) return;
        uint32_t group_ids = 1;
        for (const uint32_t slot : record_slots_) {
            count_steps(slot);
            splits_.clear();
            for (const uint32_t key : touched_keys_) {
                const auto [found, inserted] = splits_.try_emplace(
                    SplitKey{key_group_[key], key_forward_[key], key_reverse_[key]}, group_ids);
                if (inserted) ++group_ids;
                key_group_[key] = found->second;
            }
        }
        // Compact group ids to 0..groups-1.
        group_remap_.assign(group_ids, kUnassigned);
        uint32_t groups = 0;
        for (const uint32_t key : record_keys_) {
            uint32_t& dense = group_remap_[key_group_[key]];
            if (dense == kUnassigned) dense = groups++;
            key_group_[key] = dense;
        }
        stats_.groups += groups;
        stats_.max_groups = std::max<uint64_t>(stats_.max_groups, groups);

        // Per node: each visiting group once, with its step counts by HST orientation. Every
        // member of a group has the same visits here, so one member's counts stand for all.
        contribution_begin_.assign(record_slots_.size() + 1, 0);
        contributions_.clear();
        group_mark_.assign(groups, 0);
        uint32_t mark = 0;
        for (size_t index = 0; index < record_slots_.size(); ++index) {
            ++mark;
            count_steps(record_slots_[index]);
            for (const uint32_t key : touched_keys_) {
                const uint32_t group = key_group_[key];
                if (group_mark_[group] == mark) continue;
                group_mark_[group] = mark;
                contributions_.push_back({group, key_forward_[key], key_reverse_[key]});
            }
            contribution_begin_[index + 1] = contributions_.size();
        }

        const auto dp_started = std::chrono::steady_clock::now();
        stats_.grouping_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(dp_started - grouping_started).count());
        // Dynamic program in topological order. rows_[s][g]: best route prefix ending with s that
        // has visited group g (kNoRouteScore if none). open_[s][g]: s can be passed without
        // visiting g, so a later visit can start g's score from zero. Records whose values fit
        // run the same program on packed keys.
        if (packed_route_keys_enabled() && packed_bounds_hold(count)) {
            run_packed_dp(count, groups);
        } else {
            run_exact_dp(count, groups);
        }
        stats_.dp_ns += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - dp_started).count());
        for (const uint32_t key : record_keys_) {
            const RouteScore& candidate = record_best_[key_group_[key]];
            if (candidate.score != kNoRouteScore &&
                (best_[key].score == kNoRouteScore ||
                 route_score_better(candidate, best_[key]))) {
                best_[key] = candidate;
            }
        }
    }

    void run_exact_dp(size_t count, uint32_t groups) {
        rows_.resize(count);
        open_.resize(count);
        reachable_.assign(count, 0);
        record_best_.assign(groups, RouteScore{kNoRouteScore, 0, 0});
        delta_.resize(groups);
        delta_mark_.assign(groups, 0);
        uint32_t delta_epoch = 0;  // at most one increment per subpath of this record
        for (size_t subpath = 0; subpath < count; ++subpath) {
            bool reach = is_source_[subpath] != 0;
            for (size_t edge = pred_count_[subpath]; edge < pred_count_[subpath + 1]; ++edge) {
                reach = reach || reachable_[preds_[edge]] != 0;
            }
            if (reach) {
                reachable_[subpath] = 1;
                std::vector<RouteScore>& row = acquire(rows_[subpath], free_rows_);
                std::vector<char>& open = acquire(open_[subpath], free_open_);
                row.assign(groups, RouteScore{kNoRouteScore, 0, 0});
                open.assign(groups, static_cast<char>(is_source_[subpath]));
                for (size_t edge = pred_count_[subpath]; edge < pred_count_[subpath + 1]; ++edge) {
                    const uint32_t pred = preds_[edge];
                    if (!reachable_[pred]) continue;
                    const std::vector<RouteScore>& prior = rows_[pred];
                    const std::vector<char>& prior_open = open_[pred];
                    for (uint32_t group = 0; group < groups; ++group) {
                        if (route_score_better(prior[group], row[group])) row[group] = prior[group];
                        open[group] = static_cast<char>(open[group] | prior_open[group]);
                    }
                }
                stats_.dp_cells += static_cast<uint64_t>(groups) *
                    (1 + pred_count_[subpath + 1] - pred_count_[subpath]);

                // This subpath's contribution to each group it visits.
                accumulate_deltas(subpath, ++delta_epoch);
                for (const uint32_t group : touched_groups_) {
                    RouteScore base = row[group];
                    if (open[group] && route_score_better(RouteScore{}, base)) base = RouteScore{};
                    if (base.score == kNoRouteScore) {
                        throw std::logic_error("alignment-path tally reached a subpath with no route");
                    }
                    const RouteScore& delta = delta_[group];
                    row[group] = {route_checked_add(base.score, delta.score),
                                  route_checked_add(base.forward_bases, delta.forward_bases),
                                  route_checked_add(base.reverse_bases, delta.reverse_bases)};
                    open[group] = 0;
                }
                if (!has_out_[subpath]) {
                    for (uint32_t group = 0; group < groups; ++group) {
                        if (route_score_better(row[group], record_best_[group])) {
                            record_best_[group] = row[group];
                        }
                    }
                }
            }
            // Release rows no later subpath reads.
            for (size_t edge = pred_count_[subpath]; edge < pred_count_[subpath + 1]; ++edge) {
                const uint32_t pred = preds_[edge];
                if (last_use_[pred] == subpath) release(pred);
            }
            if (last_use_[subpath] == subpath) release(subpath);
        }
    }

    // Fill delta_ for every group this subpath visits and list them in touched_groups_.
    void accumulate_deltas(size_t subpath, uint32_t delta_epoch) {
        touched_groups_.clear();
        for (size_t index = visit_begin_[subpath]; index < visit_begin_[subpath + 1]; ++index) {
            const Visit& visit = visits_[index];
            const uint32_t record_slot = slot_record_index_[visit.slot];
            for (size_t c = contribution_begin_[record_slot];
                 c < contribution_begin_[record_slot + 1]; ++c) {
                const Contribution& contribution = contributions_[c];
                if (delta_mark_[contribution.group] != delta_epoch) {
                    delta_mark_[contribution.group] = delta_epoch;
                    delta_[contribution.group] = {};
                    touched_groups_.push_back(contribution.group);
                }
                RouteScore& delta = delta_[contribution.group];
                const int64_t steps = contribution.forward_steps + contribution.reverse_steps;
                const int64_t along = visit.read_reverse ? contribution.reverse_steps
                                                         : contribution.forward_steps;
                const int64_t against = steps - along;
                delta.score = route_checked_add(
                    delta.score, route_checked_multiply(visit.score, steps));
                delta.forward_bases = route_checked_add(
                    delta.forward_bases, route_checked_multiply(visit.bases, along));
                delta.reverse_bases = route_checked_add(
                    delta.reverse_bases, route_checked_multiply(visit.bases, against));
            }
        }
    }

    // Packed route keys: (score + 2^23) << 40 | total bases << 20 | forward bases. Integer order
    // equals route_score_better's order (score, then total bases, then forward bases), so the
    // hot merge is a plain max. Key 0 means no route. Valid only when every route prefix of the
    // record keeps |score| < 2^23 and total bases < 2^20, which packed_bounds_hold() proves.
    static constexpr int64_t kPackScoreOffset = int64_t{1} << 23;
    static constexpr int64_t kPackBasesLimit = int64_t{1} << 20;

    static uint64_t pack_route(const RouteScore& route) {
        return (static_cast<uint64_t>(route.score + kPackScoreOffset) << 40) |
            (static_cast<uint64_t>(route.forward_bases + route.reverse_bases) << 20) |
            static_cast<uint64_t>(route.forward_bases);
    }

    static RouteScore unpack_route(uint64_t key) {
        const int64_t total = static_cast<int64_t>((key >> 20) & (kPackBasesLimit - 1));
        const int64_t forward = static_cast<int64_t>(key & (kPackBasesLimit - 1));
        return {static_cast<int64_t>(key >> 40) - kPackScoreOffset, forward, total - forward};
    }

    // Upper bounds, over every route prefix, of the absolute score and of the aligned bases any
    // group can accumulate: longest paths over per-subpath weights that assume each visit counts
    // for the most HST steps any group takes on that node.
    bool packed_bounds_hold(size_t count) {
        slot_max_steps_.assign(record_slots_.size(), 0);
        for (size_t index = 0; index < record_slots_.size(); ++index) {
            for (size_t c = contribution_begin_[index]; c < contribution_begin_[index + 1]; ++c) {
                slot_max_steps_[index] = std::max(slot_max_steps_[index],
                    contributions_[c].forward_steps + contributions_[c].reverse_steps);
            }
        }
        bound_score_.assign(count, 0);
        bound_bases_.assign(count, 0);
        for (size_t subpath = 0; subpath < count; ++subpath) {
            int64_t score = 0, bases = 0;
            for (size_t edge = pred_count_[subpath]; edge < pred_count_[subpath + 1]; ++edge) {
                score = std::max(score, bound_score_[preds_[edge]]);
                bases = std::max(bases, bound_bases_[preds_[edge]]);
            }
            for (size_t index = visit_begin_[subpath]; index < visit_begin_[subpath + 1]; ++index) {
                const Visit& visit = visits_[index];
                const int64_t steps = slot_max_steps_[slot_record_index_[visit.slot]];
                score += (visit.score < 0 ? -visit.score : visit.score) * steps;
                bases += visit.bases * steps;
                if (score >= kPackScoreOffset || bases >= kPackBasesLimit) return false;
            }
            bound_score_[subpath] = score;
            bound_bases_[subpath] = bases;
        }
        return true;
    }

    // The dynamic program of run_record on packed keys; fills record_best_.
    void run_packed_dp(size_t count, uint32_t groups) {
        ++stats_.packed_records;
        prows_.resize(count);
        popen_.resize(count);
        reachable_.assign(count, 0);
        packed_best_.assign(groups, 0);
        delta_.resize(groups);
        delta_mark_.assign(groups, 0);
        uint32_t delta_epoch = 0;
        const uint64_t zero_route = pack_route(RouteScore{});
        for (size_t subpath = 0; subpath < count; ++subpath) {
            bool reach = is_source_[subpath] != 0;
            for (size_t edge = pred_count_[subpath]; edge < pred_count_[subpath + 1]; ++edge) {
                reach = reach || reachable_[preds_[edge]] != 0;
            }
            if (reach) {
                reachable_[subpath] = 1;
                std::vector<uint64_t>& row = acquire(prows_[subpath], free_prows_);
                std::vector<uint8_t>& open = acquire(popen_[subpath], free_popen_);
                row.assign(groups, 0);
                open.assign(groups, static_cast<uint8_t>(is_source_[subpath]));
                uint64_t* __restrict row_data = row.data();
                uint8_t* __restrict open_data = open.data();
                for (size_t edge = pred_count_[subpath]; edge < pred_count_[subpath + 1]; ++edge) {
                    const uint32_t pred = preds_[edge];
                    if (!reachable_[pred]) continue;
                    const uint64_t* __restrict prior = prows_[pred].data();
                    const uint8_t* __restrict prior_open = popen_[pred].data();
                    for (uint32_t group = 0; group < groups; ++group) {
                        row_data[group] = std::max(row_data[group], prior[group]);
                        open_data[group] = static_cast<uint8_t>(open_data[group] | prior_open[group]);
                    }
                }
                stats_.dp_cells += static_cast<uint64_t>(groups) *
                    (1 + pred_count_[subpath + 1] - pred_count_[subpath]);
                accumulate_deltas(subpath, ++delta_epoch);
                for (const uint32_t group : touched_groups_) {
                    uint64_t base = row_data[group];
                    if (open_data[group] && zero_route > base) base = zero_route;
                    if (base == 0) {
                        throw std::logic_error("alignment-path tally reached a subpath with no route");
                    }
                    const RouteScore prior = unpack_route(base);
                    const RouteScore& delta = delta_[group];
                    row_data[group] = pack_route({prior.score + delta.score,
                                                  prior.forward_bases + delta.forward_bases,
                                                  prior.reverse_bases + delta.reverse_bases});
                    open_data[group] = 0;
                }
                if (!has_out_[subpath]) {
                    uint64_t* __restrict best = packed_best_.data();
                    for (uint32_t group = 0; group < groups; ++group) {
                        best[group] = std::max(best[group], row_data[group]);
                    }
                }
            }
            for (size_t edge = pred_count_[subpath]; edge < pred_count_[subpath + 1]; ++edge) {
                const uint32_t pred = preds_[edge];
                if (last_use_[pred] == subpath) release_packed(pred);
            }
            if (last_use_[subpath] == subpath) release_packed(subpath);
        }
        record_best_.resize(groups);
        for (uint32_t group = 0; group < groups; ++group) {
            record_best_[group] = packed_best_[group] == 0
                ? RouteScore{kNoRouteScore, 0, 0} : unpack_route(packed_best_[group]);
        }
    }

    void release_packed(size_t subpath) {
        if (prows_[subpath].capacity() != 0) {
            prows_[subpath].clear();
            free_prows_.push_back(std::move(prows_[subpath]));
            prows_[subpath] = {};
        }
        if (popen_[subpath].capacity() != 0) {
            popen_[subpath].clear();
            free_popen_.push_back(std::move(popen_[subpath]));
            popen_[subpath] = {};
        }
    }

    // True when the record's reachable subpaths form one chain from a single source.
    bool single_route(size_t count) const {
        size_t sources = 0;
        for (size_t subpath = 0; subpath < count; ++subpath) {
            if (pred_count_[subpath + 1] - pred_count_[subpath] > 1) return false;
            if (is_source_[subpath]) ++sources;
        }
        if (sources != 1) return false;
        for (size_t subpath = 0; subpath < count; ++subpath) {
            if (out_degree_[subpath] > 1) return false;
        }
        return true;
    }

    // Sum each HST's steps along the single route and keep it if it beats the HST's best.
    void tally_single_route(size_t count) {
        size_t subpath = 0;
        while (!is_source_[subpath]) ++subpath;
        ++route_epoch_;
        route_keys_.clear();
        while (true) {
            for (size_t index = visit_begin_[subpath]; index < visit_begin_[subpath + 1]; ++index) {
                const Visit& visit = visits_[index];
                for (uint32_t step = slot_begin_[visit.slot]; step < slot_end_[visit.slot]; ++step) {
                    const uint32_t key = steps_[step].key;
                    if (key_route_epoch_[key] != route_epoch_) {
                        key_route_epoch_[key] = route_epoch_;
                        key_route_[key] = RouteScore{};
                        route_keys_.push_back(key);
                    }
                    RouteScore& total = key_route_[key];
                    total.score = route_checked_add(total.score, visit.score);
                    int64_t& bases = visit.read_reverse == steps_[step].path_reverse
                        ? total.forward_bases : total.reverse_bases;
                    bases = route_checked_add(bases, visit.bases);
                }
            }
            if (out_degree_[subpath] == 0) break;
            subpath = next_of_[subpath];
            if (subpath >= count) break;
        }
        for (const uint32_t key : route_keys_) {
            const RouteScore& candidate = key_route_[key];
            if (best_[key].score == kNoRouteScore || route_score_better(candidate, best_[key])) {
                best_[key] = candidate;
            }
        }
    }

    template<class T>
    static std::vector<T>& acquire(std::vector<T>& slot, std::vector<std::vector<T>>& free_list) {
        if (slot.capacity() == 0 && !free_list.empty()) {
            slot = std::move(free_list.back());
            free_list.pop_back();
        }
        return slot;
    }

    void release(size_t subpath) {
        if (rows_[subpath].capacity() != 0) {
            rows_[subpath].clear();
            free_rows_.push_back(std::move(rows_[subpath]));
            rows_[subpath] = {};
        }
        if (open_[subpath].capacity() != 0) {
            open_[subpath].clear();
            free_open_.push_back(std::move(open_[subpath]));
            open_[subpath] = {};
        }
    }

    struct SplitKey {
        uint32_t group;
        int64_t forward;
        int64_t reverse;
        bool operator==(const SplitKey&) const = default;
        template<class H>
        friend H AbslHashValue(H h, const SplitKey& key) {
            return H::combine(std::move(h), key.group, key.forward, key.reverse);
        }
    };
    static constexpr uint32_t kUnassigned = std::numeric_limits<uint32_t>::max();

    absl::flat_hash_map<Key, uint32_t, Hash> key_ids_;
    std::vector<Key> keys_;
    std::vector<RouteScore> best_;
    std::vector<uint32_t> key_group_;
    std::vector<uint64_t> key_epoch_, key_token_epoch_, key_route_epoch_;
    std::vector<RouteScore> key_route_;
    std::vector<uint32_t> route_keys_;
    std::vector<uint32_t> out_degree_;
    std::vector<size_t> next_of_;
    std::vector<int64_t> key_forward_, key_reverse_;
    absl::flat_hash_map<int64_t, uint32_t> node_slots_;
    std::vector<uint32_t> slot_begin_, slot_end_, slot_record_index_;
    std::vector<uint64_t> slot_epoch_;
    std::vector<Step> steps_;
    std::vector<int64_t> per_node_;
    std::vector<Visit> visits_;
    std::vector<size_t> visit_begin_;
    std::vector<uint32_t> record_slots_, record_keys_, touched_keys_, touched_groups_;
    absl::flat_hash_map<SplitKey, uint32_t> splits_;
    std::vector<uint32_t> group_remap_, group_mark_, delta_mark_;
    std::vector<Contribution> contributions_;
    std::vector<size_t> contribution_begin_;
    std::vector<size_t> pred_count_, pred_fill_, last_use_;
    std::vector<uint32_t> preds_;
    std::vector<char> has_out_, is_source_, reachable_;
    std::vector<std::vector<RouteScore>> rows_, free_rows_;
    std::vector<std::vector<char>> open_, free_open_;
    std::vector<std::vector<uint64_t>> prows_, free_prows_;
    std::vector<std::vector<uint8_t>> popen_, free_popen_;
    std::vector<uint64_t> packed_best_;
    std::vector<int64_t> slot_max_steps_, bound_score_, bound_bases_;
    std::vector<RouteScore> record_best_, delta_;
    uint64_t epoch_ = 0;
    uint64_t token_epoch_ = 0;
    uint64_t route_epoch_ = 0;
    RouteTallyStats stats_;
};

// D091 is the default; PANCOLLAPSE_STRUCTURAL_TALLY=summed restores D048's summed tally for
// comparison runs.
inline bool structural_tally_summed() {
    static const bool summed = [] {
        const char* mode = std::getenv("PANCOLLAPSE_STRUCTURAL_TALLY");
        if (mode == nullptr || std::string(mode) == "route_max") return false;
        if (std::string(mode) == "summed") return true;
        throw std::runtime_error("PANCOLLAPSE_STRUCTURAL_TALLY must be route_max or summed");
    }();
    return summed;
}

// Steps 2-3 (tally), reused-workspace form: clears `tallies` (keeping its backing
// storage) and refills it with each HST's best alignment path (D091, RouteTally above).
// With PANCOLLAPSE_STRUCTURAL_TALLY=summed it instead adds, across every alignment of one
// read, each node's vg score to every HST path crossing it, with orientation evidence
// (aligned bases forward vs reverse relative to the path) accumulated the same way. Callers that process many groups
// pass one long-lived TallyMap here so per-group allocation is amortized away.
inline void tally_read_group_into(
    TallyMap& tallies,
    const std::vector<const vg::MultipathAlignment*>& records,
    const PathLookup& lookup,
    const NodeScorer& node_scorer = flat_scorer()) {
    tallies.clear();
    if (!structural_tally_summed()) {
        thread_local RouteTally<std::string> route_tally;
        route_tally.run(
            records, node_scorer,
            [&](int64_t node_id, const auto& emit) {
                lookup(node_id, [&](const std::string& path_name, bool path_is_reverse) {
                    emit(path_name, path_is_reverse);
                });
            },
            [&](const std::string& path_name, const RouteScore& best) {
                tallies[path_name] = {best.score, best.forward_bases, best.reverse_bases};
            });
        return;
    }
    std::vector<int64_t> per_node;

    for (const vg::MultipathAlignment* record : records) {
        const vg::MultipathAlignment& mp = *record;
        const std::vector<size_t> offsets = subpath_read_offsets(mp);

        for (int s = 0; s < mp.subpath_size(); ++s) {
            const vg::Subpath& subpath = mp.subpath(s);
            node_scorer(mp, s, offsets[static_cast<size_t>(s)], per_node);
            const vg::Path& path = subpath.path();
            for (int i = 0; i < path.mapping_size(); ++i) {
                const vg::Mapping& mapping = path.mapping(i);
                const int64_t node_id = mapping.position().node_id();
                const bool read_is_reverse = mapping.position().is_reverse();
                const int64_t node_score = per_node[static_cast<size_t>(i)];
                const int64_t aligned_bases = mapping_aligned_bases(mapping);

                lookup(node_id, [&](const std::string& path_name, bool path_is_reverse) {
                    HstTally& tally = tallies[path_name];
                    tally.score += node_score;
                    if (read_is_reverse == path_is_reverse) {
                        tally.forward_bases += aligned_bases;
                    } else {
                        tally.reverse_bases += aligned_bases;
                    }
                });
            }
        }
    }
}

// Convenience form that allocates a fresh TallyMap. Prefer tally_read_group_into with
// a reused map on hot paths that process many groups.
inline TallyMap tally_read_group(
    const std::vector<const vg::MultipathAlignment*>& records,
    const PathLookup& lookup,
    const NodeScorer& node_scorer = flat_scorer()) {
    TallyMap tallies;
    tally_read_group_into(tallies, records, lookup, node_scorer);
    return tallies;
}

struct RadTarget {
    std::string transcript;
    bool forward = true;
    // Populated by the explicit path-identity selector. Legacy t2g callers leave
    // these empty so their historical result surface is unchanged.
    std::vector<std::string> winning_paths;
    std::vector<std::string> winning_parents;
};

struct ResolvedPathIdentity {
    std::string unique_parent;
    std::string canonical_transcript;
};

struct NumericPathIdentity {
    std::string unique_parent;
    uint32_t canonical_target_id = 0;
};

struct RankedPathIdentity {
    const std::string* path_name = nullptr;
    const std::string* unique_parent = nullptr;
    uint32_t parent_rank = 0;
    uint32_t canonical_target_id = 0;
    bool is_exon = false;
};

struct RankedParent {
    const std::string* unique_parent = nullptr;
    uint32_t parent_rank = 0;
};

// Compact production-only collapse result. Pointer vectors can be accumulated without allocating
// one ordered-tree node per winning path/Parent. The caller lexically sorts them only if that
// target/layer is ultimately emitted. Retain Parent ranks for direct metadata lookup.
struct CompactCollapsedIdentityTally {
    int64_t score = std::numeric_limits<int64_t>::min();
    int64_t forward_bases = 0;
    int64_t reverse_bases = 0;
    std::vector<const std::string*> winning_paths;
    std::vector<RankedParent> winning_parents;
};

struct CompactIdentityLayers {
    std::map<uint32_t, CompactCollapsedIdentityTally> exon;
    std::map<uint32_t, CompactCollapsedIdentityTally> body;
};

template <class IdentityResolver>
inline CompactIdentityLayers collapse_ranked_identity_tallies(
    const NumericTallyMap& tallies, IdentityResolver&& resolve_identity) {
    struct ParentEvidence {
        uint32_t target_id = 0;
        const std::string* unique_parent = nullptr;
        bool is_exon = false;
        int64_t score = std::numeric_limits<int64_t>::min();
        int64_t forward_bases = 0;
        int64_t reverse_bases = 0;
        const std::string* first_winning_path = nullptr;
        std::vector<const std::string*> additional_winning_paths;
    };
    // A layer bit keeps exon/body evidence independent without reserving two
    // full-size flat hash tables for every read group. Parent ranks are only
    // 32 bits, so the combined key remains collision-free.
    using ParentMap = absl::flat_hash_map<uint64_t, ParentEvidence>;
    ParentMap parents;
    parents.reserve(tallies.size());

    for (const auto& [path_handle, tally] : tallies) {
        const std::optional<RankedPathIdentity> identity = resolve_identity(path_handle);
        if (!identity.has_value()) {
            continue;
        }
        const uint64_t layered_parent_key =
            (static_cast<uint64_t>(identity->parent_rank) << 1U) |
            static_cast<uint64_t>(identity->is_exon);
        auto [it, inserted] = parents.try_emplace(layered_parent_key);
        if (inserted) {
            it->second.target_id = identity->canonical_target_id;
            it->second.unique_parent = identity->unique_parent;
            it->second.is_exon = identity->is_exon;
        }
        if (!inserted && it->second.target_id != identity->canonical_target_id) {
            throw std::runtime_error("unique Parent resolves to multiple canonical targets");
        }
        ParentEvidence& evidence = it->second;
        if (inserted || tally.score > evidence.score) {
            evidence.score = tally.score;
            evidence.forward_bases = tally.forward_bases;
            evidence.reverse_bases = tally.reverse_bases;
            evidence.first_winning_path = identity->path_name;
            evidence.additional_winning_paths.clear();
        } else if (tally.score == evidence.score) {
            evidence.forward_bases += tally.forward_bases;
            evidence.reverse_bases += tally.reverse_bases;
            evidence.additional_winning_paths.push_back(identity->path_name);
        }
    }

    CompactIdentityLayers result;
    for (const auto& [layered_parent_key, parent] : parents) {
        const RankedParent ranked_parent{
            parent.unique_parent, static_cast<uint32_t>(layered_parent_key >> 1U)};
        auto& targets = parent.is_exon ? result.exon : result.body;
        CompactCollapsedIdentityTally& target = targets[parent.target_id];
        if (parent.score > target.score) {
            target.score = parent.score;
            target.forward_bases = parent.forward_bases;
            target.reverse_bases = parent.reverse_bases;
            target.winning_paths.clear();
            target.winning_paths.reserve(
                1 + parent.additional_winning_paths.size());
            target.winning_paths.push_back(parent.first_winning_path);
            target.winning_paths.insert(
                target.winning_paths.end(),
                parent.additional_winning_paths.begin(),
                parent.additional_winning_paths.end());
            target.winning_parents.assign(1, ranked_parent);
        } else if (parent.score == target.score) {
            target.forward_bases += parent.forward_bases;
            target.reverse_bases += parent.reverse_bases;
            target.winning_paths.push_back(parent.first_winning_path);
            target.winning_paths.insert(
                target.winning_paths.end(),
                parent.additional_winning_paths.begin(),
                parent.additional_winning_paths.end());
            target.winning_parents.push_back(ranked_parent);
        }
    }

    return result;
}

struct CollapsedIdentityTally {
    int64_t score = std::numeric_limits<int64_t>::min();
    int64_t forward_bases = 0;
    int64_t reverse_bases = 0;
    std::set<std::string> winning_paths;
    std::set<std::string> winning_parents;
};

inline void retain_max_path_evidence(CollapsedIdentityTally& evidence,
                                     const std::string& path_name,
                                     const std::string& unique_parent,
                                     const HstTally& tally) {
    if (tally.score > evidence.score) {
        evidence.score = tally.score;
        evidence.forward_bases = tally.forward_bases;
        evidence.reverse_bases = tally.reverse_bases;
        evidence.winning_paths = {path_name};
        evidence.winning_parents = {unique_parent};
    } else if (tally.score == evidence.score) {
        evidence.forward_bases += tally.forward_bases;
        evidence.reverse_bases += tally.reverse_bases;
        evidence.winning_paths.insert(path_name);
        evidence.winning_parents.insert(unique_parent);
    }
}

inline void retain_max_parent_evidence(CollapsedIdentityTally& evidence,
                                       const std::string& unique_parent,
                                       const CollapsedIdentityTally& parent) {
    if (parent.score > evidence.score) {
        evidence = parent;
        evidence.winning_parents = {unique_parent};
    } else if (parent.score == evidence.score) {
        evidence.forward_bases += parent.forward_bases;
        evidence.reverse_bases += parent.reverse_bases;
        evidence.winning_paths.insert(parent.winning_paths.begin(), parent.winning_paths.end());
        evidence.winning_parents.insert(unique_parent);
    }
}

// Production identity collapse for one layer. Every exact path is resolved first,
// paths MAX-collapse within unique_parent, then Parents MAX-collapse within canonical
// transcript target. Missing paths are ignored so the caller can run this independently
// for exon and body layers. Scores are never summed; all tied score-winning paths and
// Parents are retained in sorted sets for BAM provenance.
template <class IdentityResolver>
inline std::map<uint32_t, CollapsedIdentityTally> collapse_identity_tallies(
    const TallyMap& tallies, IdentityResolver&& resolve_identity) {
    struct ParentEvidence {
        uint32_t target_id = 0;
        CollapsedIdentityTally evidence;
    };
    std::map<std::string, ParentEvidence> per_parent;
    for (const auto& [path_name, tally] : tallies) {
        const std::optional<NumericPathIdentity> identity = resolve_identity(path_name);
        if (!identity.has_value()) {
            continue;
        }
        auto [it, inserted] = per_parent.try_emplace(
            identity->unique_parent, ParentEvidence{identity->canonical_target_id, {}});
        if (!inserted && it->second.target_id != identity->canonical_target_id) {
            throw std::runtime_error("unique Parent resolves to multiple canonical targets");
        }
        retain_max_path_evidence(it->second.evidence, path_name, identity->unique_parent, tally);
    }

    std::map<uint32_t, CollapsedIdentityTally> per_target;
    for (const auto& [unique_parent, parent] : per_parent) {
        retain_max_parent_evidence(per_target[parent.target_id], unique_parent, parent.evidence);
    }
    return per_target;
}

// Production score-mode selector. The explicit path -> Parent -> canonical MAX
// hierarchy is observable through winning_paths/winning_parents. Only canonical
// targets tied at the global top score are emitted, preserving D048 semantics.
template <class IdentityResolver>
inline std::vector<RadTarget> select_identity_targets(const TallyMap& tallies,
                                                      IdentityResolver&& resolve_identity) {
    struct ParentEvidence {
        std::string canonical_transcript;
        CollapsedIdentityTally evidence;
    };
    std::map<std::string, ParentEvidence> per_parent;
    for (const auto& [path_name, tally] : tallies) {
        const ResolvedPathIdentity identity = resolve_identity(path_name);
        auto [it, inserted] = per_parent.try_emplace(
            identity.unique_parent, ParentEvidence{identity.canonical_transcript, {}});
        if (!inserted && it->second.canonical_transcript != identity.canonical_transcript) {
            throw std::runtime_error("unique Parent resolves to multiple canonical transcripts");
        }
        retain_max_path_evidence(it->second.evidence, path_name, identity.unique_parent, tally);
    }

    std::map<std::string, CollapsedIdentityTally> per_transcript;
    for (const auto& [unique_parent, parent] : per_parent) {
        retain_max_parent_evidence(per_transcript[parent.canonical_transcript], unique_parent,
                                   parent.evidence);
    }
    int64_t top = std::numeric_limits<int64_t>::min();
    for (const auto& [transcript, evidence] : per_transcript) {
        top = std::max(top, evidence.score);
    }

    std::vector<RadTarget> targets;
    for (const auto& [transcript, evidence] : per_transcript) {
        if (evidence.score != top) {
            continue;
        }
        targets.push_back({transcript,
                           evidence.forward_bases >= evidence.reverse_bases,
                           {evidence.winning_paths.begin(), evidence.winning_paths.end()},
                           {evidence.winning_parents.begin(), evidence.winning_parents.end()}});
    }
    return targets;
}

// Steps 3-4 (select): winners are the HSTs tied at the single top score; collapse
// them to unique transcript IDs; per transcript, orientation is the majority of
// aligned bases (forward on an exact tie); targets sorted by transcript id. The raw
// path top score is selected BEFORE transcript collapse. This order is essential when
// several graph paths alias to one transcript: their scores are alternatives and must
// never be summed to manufacture a winning transcript.
template <class TranscriptResolver>
inline std::vector<RadTarget> select_targets(const TallyMap& tallies,
                                             TranscriptResolver&& resolve_transcript) {
    std::vector<RadTarget> targets;
    if (tallies.empty()) {
        return targets;
    }

    int64_t top = std::numeric_limits<int64_t>::min();
    for (const auto& [name, tally] : tallies) {
        top = std::max(top, tally.score);
    }

    // std::map keeps transcript ids sorted, which is the RAD dictionary rule.
    std::map<std::string, std::pair<int64_t, int64_t>> per_transcript;  // forward, reverse bases
    for (const auto& [name, tally] : tallies) {
        if (tally.score != top) {
            continue;
        }
        auto& evidence = per_transcript[resolve_transcript(name)];
        evidence.first += tally.forward_bases;
        evidence.second += tally.reverse_bases;
    }

    for (const auto& [transcript, evidence] : per_transcript) {
        targets.push_back({transcript, evidence.first >= evidence.second, {}, {}});
    }
    return targets;
}

// Backward-compatible two-column t2g behavior: derive transcript identity from the
// conventional trailing haplotype-copy suffix.
inline std::vector<RadTarget> select_targets(const TallyMap& tallies) {
    return select_targets(tallies, [](const std::string& name) { return transcript_id_of(name); });
}

}  // namespace pathtally
