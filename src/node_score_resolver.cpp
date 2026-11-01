#include "node_score_resolver.hpp"
#include "compact_score_frontier.hpp"
#include "pathtally.hpp"
#include "threshold_balance.hpp"

#include <algorithm>
#include <array>
#include <compare>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <tuple>
#include <unordered_map>

namespace pancollapse {
namespace {
using namespace direct_count;
constexpr std::int64_t no_score = std::numeric_limits<std::int64_t>::min();
constexpr std::uint32_t exon_flags = 3, body_flags = 12;

struct Endpoint {
    std::uint64_t oriented = 0;
};
struct Model {
    count_reference::ModelRecord record;
    std::vector<std::uint32_t> support_sets;
};
struct Delta {
    std::uint32_t model = 0;
    std::int64_t score = 0, gene_score = 0;
    std::uint64_t exon = 0, body = 0, forward = 0, reverse = 0;
    std::vector<std::uint32_t> support_slots;
    bool touched = false;
};
struct Veto { std::uint32_t model = 0; bool forward = false, reverse = false; };
using EdgeEffects = std::vector<Veto>;
struct Credit {
    std::uint64_t node = 0, bases = 0;
    std::int64_t score = 0;
    bool reverse = false;
};
struct Piece {
    std::uint64_t bases = 0;
    std::optional<Endpoint> first, last;
    std::vector<Credit> credits;
    std::vector<const EdgeEffects*> internal_edges;
};
struct LocalPosting {
    std::uint32_t local_model = 0, support_set = 0, flags = 0;
};
struct LocalPostingList {
    std::vector<LocalPosting> postings;
    bool has_annotation = false;
};
struct Node {
    std::uint64_t length = 0;
    const LocalPostingList* posting_list = nullptr;
};
struct Winner {
    std::int64_t score = no_score;
    std::vector<std::uint32_t> support_sets;
};
using WinnerKey = std::tuple<std::uint32_t, std::uint32_t, std::size_t>;

// Equal keys have identical future eligibility for any common GAMP suffix.
// The last endpoint survives reference-empty bridges, not reference walks.
struct StateKey {
    std::int64_t exon_balance = 0, strand_balance = 0;
    std::optional<std::uint64_t> last;
    std::uint8_t valid = 3;
    bool all_exonic = true, body_seen = false, has_bases = false;
    auto operator<=>(const StateKey&) const = default;
};
struct StateValue {
    std::int64_t score = no_score, gene_score = no_score;
    // Only the annotation-score objective emits variable body support. Gene
    // fallback has no support payload in the frozen node-score-v1 adapter.
    std::vector<std::uint32_t> support_slots;
};
struct FixedScores { std::int64_t score = 0, gene_score = 0; };
using Op = std::tuple<std::uint32_t, std::int64_t, std::int64_t, std::uint64_t,
                      std::uint64_t, std::uint64_t, std::uint64_t,
                      std::vector<std::uint32_t>>;
using SpliceOp = std::pair<std::uint64_t, std::uint8_t>;
using Signature = std::tuple<std::uint32_t, bool, std::vector<Op>, std::vector<SpliceOp>>;
using Membership = std::tuple<std::uint64_t, std::uint32_t, std::uint32_t>;
using MembershipSignature = std::tuple<std::uint32_t, std::uint8_t,
                                     std::vector<Membership>, std::vector<SpliceOp>>;
struct MemberClass {
    std::vector<std::uint32_t> aliases;
    std::vector<Op> operations;
    std::vector<SpliceOp> splices;
    bool fixed_support = false;
};
struct ClassPosting {
    std::uint32_t class_id = 0, flags = 0, support_slot = 0;
};

std::uint64_t saturated_add(std::uint64_t a, std::uint64_t b) {
    return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}
std::int64_t signed_bases(std::uint64_t value) {
    if (value > static_cast<std::uint64_t>(INT64_MAX))
        throw std::overflow_error("node-score base-count overflow");
    return static_cast<std::int64_t>(value);
}

std::uint64_t bases_of(const vg::Mapping& mapping) {
    std::uint64_t bases = 0;
    for (const auto& edit : mapping.edit()) bases += edit.from_length();
    return bases;
}
template<class T> void unique(std::vector<T>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}
void union_slots(std::vector<std::uint32_t>& into, const std::vector<std::uint32_t>& from) {
    into.insert(into.end(), from.begin(), from.end());
    unique(into);
}
void merge_value(StateValue& into, const StateValue& from) {
    if (from.score > into.score) { into.score = from.score; into.support_slots = from.support_slots; }
    else if (from.score == into.score) union_slots(into.support_slots, from.support_slots);
    into.gene_score = std::max(into.gene_score, from.gene_score);
}
void atomic_max(std::atomic<std::uint64_t>& value, std::uint64_t next) {
    auto old = value.load(std::memory_order_relaxed);
    while (old < next && !value.compare_exchange_weak(old, next, std::memory_order_relaxed)) {}
}
void atomic_saturated_add(std::atomic<std::uint64_t>& value, std::uint64_t delta) {
    auto old = value.load(std::memory_order_relaxed);
    while (!value.compare_exchange_weak(old, saturated_add(old, delta), std::memory_order_relaxed)) {}
}
}  // namespace

std::vector<NumericResolvedAssignment> NodeScoreResolver::resolve(
    const std::vector<const vg::MultipathAlignment*>& records,
    std::span<const NumericAssignmentProfile> profiles) const {
    reference_.begin_read_group();
    std::unordered_map<std::uint64_t, Node> nodes;
    std::unordered_map<std::uint32_t, std::uint32_t> model_index;
    std::vector<Model> models;
    std::deque<LocalPostingList> decoded_membership_lists;
    std::unordered_map<std::uint32_t, const LocalPostingList*> shared_membership_lists;
    std::uint64_t nodes_with_shared_id = 0, membership_lists_decoded = 0, membership_lists_reused = 0;
    std::uint64_t posting_rows_decoded = 0, posting_rows_reused = 0;
    // Annotation lookup is paid once per distinct node, never once per route.
    for (const auto* alignment : records) {
        if (!alignment) throw std::runtime_error("node-score received null GAMP record");
        for (const auto& subpath : alignment->subpath()) for (const auto& mapping : subpath.path().mapping()) {
            const auto id = static_cast<std::uint64_t>(mapping.position().node_id());
            if (nodes.contains(id)) continue;
            const auto record = reference_.node_view(id);
            if (!record) throw std::runtime_error("GAMP node is absent from prepared count reference: " + std::to_string(id));
            Node node;
            node.length = record->length;
            const auto decode = [&] {
                decoded_membership_lists.emplace_back();
                auto& decoded = decoded_membership_lists.back();
                decoded.postings.reserve(record->postings.size());
                for (const auto posting : record->postings) {
                    auto [found, inserted] = model_index.try_emplace(posting.model, models.size());
                    if (inserted) {
                        Model model;
                        model.record = reference_.model(posting.model);
                        models.push_back(std::move(model));
                    }
                    decoded.postings.push_back({found->second, posting.support_set, posting.flags});
                    decoded.has_annotation = decoded.has_annotation ||
                        (posting.flags & (exon_flags | body_flags));
                    if (posting.flags & body_flags) models[found->second].support_sets.push_back(posting.support_set);
                }
                ++membership_lists_decoded;
                posting_rows_decoded += decoded.postings.size();
                return &decoded;
            };
            if (record->shared_membership_id == count_reference::missing) {
                node.posting_list = decode();
            } else {
                ++nodes_with_shared_id;
                auto [found, inserted] = shared_membership_lists.try_emplace(record->shared_membership_id);
                if (inserted) {
                    node.posting_list = found->second = decode();
                } else {
                    node.posting_list = found->second;
                    ++membership_lists_reused;
                    posting_rows_reused += node.posting_list->postings.size();
                }
            }
            nodes.emplace(id, std::move(node));
        }
    }
    std::map<WinnerKey, Winner> winners;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> transcript_models;
    for (std::uint32_t index = 0; index < models.size(); ++index) {
        auto& model = models[index];
        unique(model.support_sets);
        transcript_models[model.record.transcript_string].push_back(index);
    }
    std::map<std::pair<std::uint64_t, std::uint64_t>, EdgeEffects> edges;
    const auto edge_effects = [&](const Endpoint& from, const Endpoint& to) -> const EdgeEffects* {
        const auto key = std::pair(from.oriented, to.oriented);
        auto [found, inserted] = edges.try_emplace(key);
        if (inserted) {
            const auto targets = reference_.splice_targets(from.oriented >> 1, to.oriented >> 1);
            if (!targets.empty()) {
                const auto owners = reference_.junction_owners(from.oriented, to.oriented);
                for (const auto transcript : targets) {
                    const auto owner = std::lower_bound(owners.begin(), owners.end(), transcript,
                        [](const auto& row, std::uint32_t id) { return row.transcript_string < id; });
                    const auto flags = owner != owners.end() && owner->transcript_string == transcript ? owner->flags : 0;
                    const auto group = transcript_models.find(transcript);
                    if (group != transcript_models.end()) for (const auto model : group->second)
                        found->second.push_back({model, (flags & 1U) == 0, (flags & 2U) == 0});
                }
            }
            std::sort(found->second.begin(), found->second.end(),
                [](const auto& a, const auto& b) { return a.model < b.model; });
        }
        return &found->second;
    };
    const auto allowed = [](const EdgeEffects* effects, std::uint32_t model) -> std::uint8_t {
        const auto row = std::lower_bound(effects->begin(), effects->end(), model,
            [](const auto& entry, std::uint32_t id) { return entry.model < id; });
        if (row == effects->end() || row->model != model) return 3;
        return static_cast<std::uint8_t>((row->forward ? 0 : 1) | (row->reverse ? 0 : 2));
    };
    const auto update_winner = [&](const Model& model, std::size_t pool, std::int64_t score,
                                   const std::vector<std::uint32_t>& slots) {
        if (score == no_score) return;
        const auto policy = pool < 4 ? model.record.exon_policy : model.record.body_policy;
        auto& winner = winners.try_emplace(
            WinnerKey{model.record.transcript_string, policy, pool}).first->second;
        if (score < winner.score) return;
        if (score > winner.score) { winner.score = score; winner.support_sets.clear(); }
        if (pool / 2 == 2) for (const auto slot : slots) {
            const auto support = model.support_sets.at(slot);
            if (std::find(winner.support_sets.begin(), winner.support_sets.end(), support) == winner.support_sets.end())
                winner.support_sets.push_back(support);
        }
    };
    std::uint64_t alignments_scored = 0, piece_visits = 0, model_visits = 0;
    std::uint64_t membership_groups = 0, raw_model_transfers = 0;
    std::uint64_t frontier_compactions = 0;
    std::uint64_t balance_saturated_groups = 0, balance_unsaturated_groups = 0;
    std::uint64_t dp_states = 0, dp_transitions = 0, max_frontier = 0, group_count = 0, offset_conflicts = 0;
    for (const auto* alignment : records) {
        const auto size = static_cast<std::size_t>(alignment->subpath_size());
        if (!size) continue;
        const auto offsets = pathtally::subpath_read_offsets(*alignment);
        std::vector<Piece> pieces(size);
        std::vector<bool> incoming(size, false);
        std::vector<std::uint64_t> record_nodes;
        bool has_annotation = false;
        for (std::size_t index = 0; index < size; ++index) {
            const auto& subpath = alignment->subpath(static_cast<int>(index));
            auto& piece = pieces[index];
            std::vector<std::int64_t> scores;
            pathtally::flat_scorer()(*alignment, static_cast<int>(index), offsets[index], scores);
            for (int m = 0; m < subpath.path().mapping_size(); ++m) {
                const auto& mapping = subpath.path().mapping(m);
                const auto id = static_cast<std::uint64_t>(mapping.position().node_id());
                const auto& node = nodes.at(id);
                record_nodes.push_back(id);
                const auto bases = bases_of(mapping);
                const auto offset = static_cast<std::uint64_t>(mapping.position().offset());
                if (offset > node.length || bases > node.length - offset)
                    throw std::runtime_error("GAMP mapping exceeds prepared node " + std::to_string(id));
                if (!bases) continue;
                const bool reverse = mapping.position().is_reverse();
                const Endpoint endpoint{(id << 1) | reverse};
                if (!piece.first) piece.first = endpoint;
                if (piece.last) piece.internal_edges.push_back(edge_effects(*piece.last, endpoint));
                piece.last = endpoint;
                piece.bases += bases;
                piece.credits.push_back({id, bases, scores[m], reverse});
                has_annotation = has_annotation || node.posting_list->has_annotation;
            }
            const auto mark = [&](std::uint32_t next) {
                if (next >= size || next <= index) throw std::runtime_error("GAMP subpaths are not topologically ordered");
                incoming[next] = true;
            };
            for (const auto next : subpath.next()) mark(next);
            for (const auto& connection : subpath.connection()) mark(connection.next());
        }
        piece_visits += size;
        std::vector<std::uint32_t> starts(alignment->start().begin(), alignment->start().end());
        if (starts.empty()) for (std::uint32_t i = 0; i < size; ++i) if (!incoming[i]) starts.push_back(i);
        for (const auto start : starts) if (start >= size) throw std::runtime_error("GAMP start index is out of range");

        // Count represented routes cheaply for telemetry only; never enumerate.
        std::vector<std::uint64_t> routes(size, 0);
        for (std::size_t i = size; i-- > 0;) {
            const auto& subpath = alignment->subpath(static_cast<int>(i));
            if (!subpath.next_size() && !subpath.connection_size()) routes[i] = 1;
            for (const auto next : subpath.next()) routes[i] = saturated_add(routes[i], routes[next]);
            for (const auto& edge : subpath.connection()) routes[i] = saturated_add(routes[i], routes[edge.next()]);
        }
        for (const auto start : starts) alignments_scored = saturated_add(alignments_scored, routes[start]);

        // Preserve the existing per-piece scorer's read-offset interpretation.
        // Report nonconvergent offsets instead of adding a biological rejection.
        std::vector<bool> reachable(size, false);
        for (const auto start : starts) reachable[start] = true;
        for (std::size_t i = 0; i < size; ++i) if (reachable[i]) {
            const auto& subpath = alignment->subpath(static_cast<int>(i));
            const auto expected = offsets[i] + pathtally::subpath_read_length(subpath);
            const auto check = [&](std::uint32_t next) {
                offset_conflicts += offsets[next] != expected;
                reachable[next] = true;
            };
            for (const auto next : subpath.next()) check(next);
            for (const auto& edge : subpath.connection()) check(edge.next());
        }

        // No model can emit evidence from this record; avoid annotation-free
        // cross-edge lookups while preserving route/offset telemetry above.
        if (!has_annotation) continue;

        // Collect every splice effect a complete route can encounter, without
        // enumerating routes. Empty pieces carry predecessor endpoints; a
        // reference-consuming piece replaces them with its own last endpoint.
        std::vector<std::vector<std::uint64_t>> endpoints(size);
        std::vector<const EdgeEffects*> local_edges;
        for (std::size_t i = 0; i < size; ++i) if (reachable[i]) {
            auto previous = std::move(endpoints[i]);
            unique(previous);
            const auto& piece = pieces[i];
            local_edges.insert(local_edges.end(), piece.internal_edges.begin(), piece.internal_edges.end());
            if (piece.first) for (const auto last : previous)
                local_edges.push_back(edge_effects(Endpoint{last}, *piece.first));
            if (piece.last) previous = {piece.last->oriented};
            const auto send = [&](std::uint32_t next) {
                endpoints[next].insert(endpoints[next].end(), previous.begin(), previous.end());
            };
            const auto& subpath = alignment->subpath(static_cast<int>(i));
            for (const auto next : subpath.next()) send(next);
            for (const auto& edge : subpath.connection()) send(edge.next());
        }
        // Ordinals only identify effects inside this read/record. Their order
        // has no bearing on winners, identities, or persisted output ordering.
        std::sort(local_edges.begin(), local_edges.end(), std::less<const EdgeEffects*>{});
        local_edges.erase(std::unique(local_edges.begin(), local_edges.end()), local_edges.end());
        std::vector<std::vector<SpliceOp>> splice_operations(models.size());
        for (std::size_t edge = 0; edge < local_edges.size(); ++edge)
            for (const auto& effect : *local_edges[edge]) {
                const auto mask = static_cast<std::uint8_t>((effect.forward ? 0 : 1) |
                                                           (effect.reverse ? 0 : 2));
                if (mask != 3) splice_operations[effect.model].emplace_back(edge, mask);
            }

        // Refine raw aliases on distinct node incidence before constructing
        // mapping-by-model tallies. These labels determine every later Delta.
        // Zero-base mappings still contributed to read-wide support modes.
        unique(record_nodes);
        std::vector<std::vector<Membership>> memberships(models.size());
        for (const auto id : record_nodes) {
            const auto& node = nodes.at(id);
            for (const auto posting : node.posting_list->postings) {
                if (!(posting.flags & (exon_flags | body_flags))) continue;
                const auto& support = models[posting.local_model].support_sets;
                const auto slot = posting.flags & body_flags ? static_cast<std::uint32_t>(
                    std::lower_bound(support.begin(), support.end(), posting.support_set) - support.begin()) : 0;
                memberships[posting.local_model].emplace_back(id, posting.flags, slot);
            }
        }
        std::map<MembershipSignature, std::vector<std::uint32_t>> partitions;
        for (std::uint32_t model = 0; model < models.size(); ++model) {
            if (memberships[model].empty()) continue;
            const std::uint8_t mode = models[model].support_sets.empty() ? 0 :
                models[model].support_sets.size() == 1 ? 1 : 2;
            // Mixed-support models retain the full singleton path. Fixed
            // aliases normalize their one slot but restore the real ID later.
            const auto identity = execution_ == NodeScoreExecution::force_sparse || mode == 2 ? model + 1 : 0;
            partitions[{identity, mode, std::move(memberships[model]),
                        std::move(splice_operations[model])}].push_back(model);
        }
        std::vector<MemberClass> classes;
        std::unordered_map<std::uint64_t, std::vector<ClassPosting>> class_nodes;
        for (auto& [signature, aliases] : partitions) {
            const auto class_id = static_cast<std::uint32_t>(classes.size());
            for (const auto& [node, flags, slot] : std::get<2>(signature))
                class_nodes[node].push_back({class_id, flags, slot});
            classes.push_back({std::move(aliases), {}, std::get<3>(signature), std::get<1>(signature) == 1});
        }
        membership_groups += classes.size();
        std::vector<Delta> deltas(classes.size());
        std::vector<std::uint32_t> touched_classes;
        for (std::uint32_t i = 0; i < size; ++i) {
            touched_classes.clear();
            for (const auto& credit : pieces[i].credits) {
                const auto found = class_nodes.find(credit.node);
                if (found == class_nodes.end()) continue;
                for (const auto& posting : found->second) {
                    auto& delta = deltas[posting.class_id];
                    if (!delta.touched) {
                        delta.model = posting.class_id;
                        delta.score = delta.gene_score = 0;
                        delta.exon = delta.body = delta.forward = delta.reverse = 0;
                        delta.support_slots.clear();
                        delta.touched = true;
                        touched_classes.push_back(posting.class_id);
                    }
                    delta.score = pathtally::route_checked_add(delta.score, credit.score);
                    const bool exon = (posting.flags & exon_flags) != 0;
                    const bool body = (posting.flags & body_flags) != 0;
                    if (exon) delta.exon += credit.bases;
                    if (body) {
                        delta.body += credit.bases;
                        delta.gene_score = pathtally::route_checked_add(delta.gene_score, credit.score);
                        if (!classes[posting.class_id].fixed_support)
                            delta.support_slots.push_back(posting.support_slot);
                    }
                    const auto flags = posting.flags & (exon ? exon_flags : body_flags);
                    if (flags & (credit.reverse ? 10U : 5U)) delta.forward += credit.bases;
                    else delta.reverse += credit.bases;
                }
            }
            std::sort(touched_classes.begin(), touched_classes.end());
            for (const auto class_id : touched_classes) {
                auto& delta = deltas[class_id];
                unique(delta.support_slots);
                classes[class_id].operations.emplace_back(i, delta.score, delta.gene_score, delta.exon,
                    delta.body, delta.forward, delta.reverse, std::move(delta.support_slots));
                ++model_visits;
                raw_model_transfers += classes[class_id].aliases.size();
                delta.touched = false;
            }
        }
        // Different incidence classes can still yield the same complete piece
        // transfers. Preserve the second exact quotient to avoid extra DP work.
        std::map<Signature, std::vector<std::uint32_t>> groups;
        for (std::uint32_t id = 0; id < classes.size(); ++id) {
            auto& member = classes[id];
            if (member.operations.empty()) continue;
            const auto model = member.aliases.front();
            const auto identity = execution_ == NodeScoreExecution::force_sparse ||
                models[model].support_sets.size() > 1 ? model + 1 : 0;
            groups[{identity, member.fixed_support, std::move(member.operations),
                    std::move(member.splices)}].push_back(id);
        }
        group_count += groups.size();
        for (const auto& [signature, aliases] : groups) {
            const auto representative = classes[aliases.front()].aliases.front();
            // Native pairs have a fixed body-support set: body_seen already
            // determines this payload, so no per-state vector is necessary.
            const bool fixed_support = std::get<1>(signature);
            const std::vector<std::uint32_t> fixed_slots{0};
            std::vector<const Op*> by_piece(size, nullptr);
            for (const auto& op : std::get<2>(signature)) by_piece[std::get<0>(op)] = &op;
            std::vector<threshold_balance::Bounds> balance_bounds;
            if (execution_ == NodeScoreExecution::automatic) {
                std::vector<threshold_balance::Delta> deltas;
                deltas.reserve(size);
                for (std::size_t i = 0; i < size; ++i) {
                    const auto* op = by_piece[i];
                    const auto delta = threshold_balance::make_delta(
                        op ? std::get<3>(*op) : 0, pieces[i].bases,
                        op ? std::get<5>(*op) : 0, op ? std::get<6>(*op) : 0,
                        op ? std::get<1>(*op) : 0, op ? std::get<2>(*op) : 0);
                    if (!delta) break;
                    deltas.push_back(*delta);
                }
                if (deltas.size() == size)
                    balance_bounds = threshold_balance::prepare(deltas, [&](std::size_t i, const auto& visit) {
                        const auto& subpath = alignment->subpath(static_cast<int>(i));
                        for (const auto next : subpath.next()) visit(next);
                        for (const auto& edge : subpath.connection()) visit(edge.next());
                    });
                if (balance_bounds.empty()) ++balance_unsaturated_groups;
                else ++balance_saturated_groups;
            }
            const auto normalize = [&](StateKey& key, std::uint32_t next) {
                if (!balance_bounds.empty()) {
                    key.exon_balance = balance_bounds[next].exon.normalize(key.exon_balance);
                    key.strand_balance = balance_bounds[next].strand.normalize(key.strand_balance);
                }
            };
            std::vector<std::uint8_t> internal_masks(size, 3);
            for (std::size_t i = 0; i < size; ++i)
                for (const auto* effect : pieces[i].internal_edges)
                    internal_masks[i] &= allowed(effect, representative);
            std::array<Winner, 8> local;
            const auto retain = [&](std::size_t pool, std::int64_t score,
                                    const std::vector<std::uint32_t>& slots) {
                auto& winner = local[pool];
                if (score < winner.score) return;
                if (score > winner.score) { winner.score = score; winner.support_sets.clear(); }
                if (pool / 2 == 2) union_slots(winner.support_sets, slots);
            };
            const bool compact = execution_ == NodeScoreExecution::automatic &&
                models[representative].support_sets.size() <= 1;
            std::vector<std::map<StateKey, StateValue>> frontiers(compact ? 0 : size);
            std::vector<CompactScoreFrontier<StateKey>> compact_frontiers;
            if (compact) {
                compact_frontiers.reserve(size);
                for (std::size_t i = 0; i < size; ++i) compact_frontiers.emplace_back(frontier_batch_);
            }
            for (const auto start : starts) {
                StateKey key;
                normalize(key, start);
                if (compact) compact_frontiers[start].add(key, 0, 0);
                else merge_value(frontiers[start][key], StateValue{0, 0, {}});
            }
            for (std::size_t i = 0; i < size; ++i) {
                const auto& piece = pieces[i];
                const auto* op = by_piece[i];
                const auto exon = op ? std::get<3>(*op) : 0;
                const auto margin = pathtally::route_checked_add(signed_bases(exon),
                    signed_bases(exon) - signed_bases(piece.bases));
                const auto strand_margin = op ? signed_bases(std::get<5>(*op)) -
                    signed_bases(std::get<6>(*op)) : 0;
                const auto step = [&](const StateKey& prefix, auto value) {
                    ++dp_states;
                    auto key = prefix;
                    key.exon_balance = pathtally::route_checked_add(key.exon_balance, margin);
                    key.strand_balance = pathtally::route_checked_add(key.strand_balance, strand_margin);
                    key.all_exonic = key.all_exonic && exon == piece.bases;
                    key.body_seen = key.body_seen || (op && std::get<4>(*op));
                    key.has_bases = key.has_bases || piece.bases;
                    key.valid &= internal_masks[i];
                    if (key.last && piece.first) key.valid &= allowed(
                        edge_effects(Endpoint{*key.last}, *piece.first), representative);
                    // No continuation can restore a direction forbidden by a splice.
                    // Rejected alignments need no shadow assignment for counting.
                    if (key.valid == 0) return;
                    if (piece.last) key.last = piece.last->oriented;
                    if (op) {
                        value.score = pathtally::route_checked_add(value.score, std::get<1>(*op));
                        value.gene_score = pathtally::route_checked_add(value.gene_score, std::get<2>(*op));
                        if constexpr (requires { value.support_slots; })
                            if (!fixed_support) union_slots(value.support_slots, std::get<7>(*op));
                    }
                    const auto& subpath = alignment->subpath(static_cast<int>(i));
                    if (!subpath.next_size() && !subpath.connection_size() && key.has_bases) {
                        const std::size_t strand = key.strand_balance >= 0 ? 0 : 1;
                        const bool rejected = (key.valid & (1U << strand)) == 0;
                        const std::size_t tier = key.all_exonic ? 0 : key.exon_balance >= 0 ? 1 : 2;
                        if (!rejected && (tier != 2 || key.body_seen)) {
                            if constexpr (requires { value.support_slots; })
                                retain(2 * tier + strand, value.score,
                                    fixed_support ? fixed_slots : value.support_slots);
                            else retain(2 * tier + strand, value.score, fixed_slots);
                        }
                        if (!rejected && key.body_seen) retain(6 + strand, value.gene_score, {});
                    }
                    const auto send = [&](std::uint32_t next) {
                        ++dp_transitions;
                        auto successor_key = key;
                        normalize(successor_key, next);
                        if constexpr (requires { value.support_slots; })
                            merge_value(frontiers[next][successor_key], value);
                        else compact_frontiers[next].add(successor_key, value.score, value.gene_score);
                    };
                    for (const auto next : subpath.next()) send(next);
                    // Connection penalties are placement scores, not node credits.
                    for (const auto& edge : subpath.connection()) send(edge.next());
                };
                if (compact) {
                    auto states = compact_frontiers[i].take_sorted();
                    frontier_compactions += compact_frontiers[i].compactions();
                    max_frontier = std::max<std::uint64_t>(max_frontier, states.size());
                    for (const auto& cell : states)
                        step(cell.key, FixedScores{cell.score, cell.gene_score});
                } else {
                    auto states = std::move(frontiers[i]); // Release after its final consumer.
                    max_frontier = std::max<std::uint64_t>(max_frontier, states.size());
                    for (const auto& [key, value] : states) step(key, value);
                }
            }
            for (const auto member : aliases) for (const auto alias : classes[member].aliases)
                for (std::size_t pool = 0; pool != 8; ++pool) {
                    const auto& winner = local[pool];
                    if (winner.score != no_score) update_winner(models[alias], pool, winner.score,
                        winner.support_sets);
                }
        }
    }
    std::vector<NumericResolvedAssignment> result(profiles.size());
    std::array<std::int64_t, 8> tops;
    tops.fill(no_score);
    std::map<std::pair<std::uint32_t, std::size_t>, std::int64_t> transcript_tops;
    for (const auto& [key, winner] : winners) {
        auto [found, inserted] = transcript_tops.try_emplace(
            std::pair(std::get<0>(key), std::get<2>(key)), winner.score);
        if (!inserted) found->second = std::max(found->second, winner.score);
        tops[std::get<2>(key)] = std::max(tops[std::get<2>(key)], winner.score);
    }
    for (const auto& [key, winner] : winners) {
        const auto pool = std::get<2>(key);
        const auto cutoff = tops[pool] < no_score + 5 ? no_score : tops[pool] - 5;
        if (winner.score == no_score || winner.score < cutoff ||
            winner.score != transcript_tops.at({std::get<0>(key), pool})) continue;
        const auto policy = std::get<1>(key);
        if (policy == count_reference::missing) continue;
        std::vector<std::uint32_t> supports;
        for (const auto set : winner.support_sets) {
            if (set >= reference_.sets().size()) throw std::runtime_error("invalid node-score support set");
            const auto& units = reference_.sets()[set];
            supports.insert(supports.end(), units.begin(), units.end());
        }
        unique(supports);
        const auto tier = pool / 2 == 0 ? EvidenceTier::exon : pool / 2 == 1 ?
            EvidenceTier::partial_exon : pool / 2 == 2 ? EvidenceTier::body : EvidenceTier::gene;
        const auto strand = pool % 2 == 0 ? EvidenceStrand::forward : EvidenceStrand::reverse;
        for (std::size_t profile = 0; profile < profiles.size(); ++profile) {
            auto candidate = numeric_assignment_candidate(reference_, profiles[profile], policy,
                winner.score, tier, strand, supports);
            auto& facts = result[profile].facts;
            auto& destination = pool < 6 ? facts.exact : facts.gene_fallback;
            destination.push_back(std::move(candidate));
        }
    }
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        result[i].result = resolve_numeric_assignment(profiles[i], result[i].facts, true);
    }
    atomic_saturated_add(complete_alignments_, alignments_scored);
    subpath_visits_.fetch_add(piece_visits, std::memory_order_relaxed);
    model_visits_.fetch_add(model_visits, std::memory_order_relaxed);
    dp_states_.fetch_add(dp_states, std::memory_order_relaxed);
    dp_transitions_.fetch_add(dp_transitions, std::memory_order_relaxed);
    atomic_max(max_frontier_states_, max_frontier);
    raw_models_.fetch_add(models.size(), std::memory_order_relaxed);
    model_groups_.fetch_add(group_count, std::memory_order_relaxed);
    read_offset_conflicts_.fetch_add(offset_conflicts, std::memory_order_relaxed);
    membership_groups_.fetch_add(membership_groups, std::memory_order_relaxed);
    raw_model_transfers_.fetch_add(raw_model_transfers, std::memory_order_relaxed);
    frontier_compactions_.fetch_add(frontier_compactions, std::memory_order_relaxed);
    balance_saturated_groups_.fetch_add(balance_saturated_groups, std::memory_order_relaxed);
    balance_unsaturated_groups_.fetch_add(balance_unsaturated_groups, std::memory_order_relaxed);
    nodes_with_shared_id_.fetch_add(nodes_with_shared_id, std::memory_order_relaxed);
    membership_lists_decoded_.fetch_add(membership_lists_decoded, std::memory_order_relaxed);
    membership_lists_reused_.fetch_add(membership_lists_reused, std::memory_order_relaxed);
    posting_rows_decoded_.fetch_add(posting_rows_decoded, std::memory_order_relaxed);
    posting_rows_reused_.fetch_add(posting_rows_reused, std::memory_order_relaxed);
    return result;
}
}  // namespace pancollapse
