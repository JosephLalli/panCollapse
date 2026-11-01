// Frozen complete-route implementation for bounded differential tests only.
#include "node_score_test_support.hpp"
#include "pathtally.hpp"

#include <algorithm>
#include <array>
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
struct Tally {
    std::int64_t score = 0, gene_score = 0;
    std::uint64_t exon = 0, body = 0, forward = 0, reverse = 0;
    std::uint32_t wrong_forward = 0, wrong_reverse = 0;
    std::vector<std::uint32_t> support_counts;
};
struct Model {
    count_reference::ModelRecord record;
    std::vector<std::uint32_t> support_sets;
    std::array<std::size_t, 8> winners{};
    Tally tally;
};
struct Delta {
    std::uint32_t model = 0;
    std::int64_t score = 0, gene_score = 0;
    std::uint64_t exon = 0, body = 0, forward = 0, reverse = 0;
    std::vector<std::uint32_t> support_slots;
};
struct Veto { std::uint32_t model = 0; bool forward = false, reverse = false; };
using EdgeEffects = std::vector<Veto>;
struct Piece {
    std::uint64_t bases = 0;
    std::optional<Endpoint> first, last;
    std::vector<Delta> deltas;
    std::vector<const EdgeEffects*> internal_edges;
};
struct Node {
    std::uint64_t length = 0;
    std::vector<count_reference::Posting> postings;
};
struct Winner {
    std::int64_t score = no_score;
    std::vector<std::uint32_t> support_sets;
};
using WinnerKey = std::tuple<std::uint32_t, std::uint32_t, std::size_t>;

std::uint64_t bases_of(const vg::Mapping& mapping) {
    std::uint64_t bases = 0;
    for (const auto& edit : mapping.edit()) bases += edit.from_length();
    return bases;
}
template<class T> void unique(std::vector<T>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}
}  // namespace

std::vector<ResolvedAssignment> enumerate_node_score(
    const count_reference::Source& reference_,
    const std::vector<const vg::MultipathAlignment*>& records,
    std::span<const EffectiveProfile> profiles) {
    reference_.begin_read_group();
    std::unordered_map<std::uint64_t, Node> nodes;
    std::unordered_map<std::uint32_t, std::uint32_t> model_index;
    std::vector<Model> models;
    // Annotation lookup is paid once per distinct node, never once per route.
    for (const auto* alignment : records) {
        if (!alignment) throw std::runtime_error("node-score received null GAMP record");
        for (const auto& subpath : alignment->subpath()) for (const auto& mapping : subpath.path().mapping()) {
            const auto id = static_cast<std::uint64_t>(mapping.position().node_id());
            if (nodes.contains(id)) continue;
            const auto* record = reference_.node(id);
            if (!record) throw std::runtime_error("GAMP node is absent from prepared count reference: " + std::to_string(id));
            Node node{record->length, {}};
            const auto postings = reference_.node_postings(*record);
            node.postings.assign(postings.begin(), postings.end());
            for (auto& posting : node.postings) {
                auto [found, inserted] = model_index.try_emplace(posting.model, models.size());
                if (inserted) {
                    Model model;
                    model.record = reference_.model(posting.model);
                    models.push_back(std::move(model));
                }
                posting.model = found->second;
                if (posting.flags & body_flags) models[posting.model].support_sets.push_back(posting.support_set);
            }
            nodes.emplace(id, std::move(node));
        }
    }
    std::map<WinnerKey, std::size_t> winner_index;
    std::vector<Winner> winners;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> transcript_models;
    for (std::uint32_t index = 0; index < models.size(); ++index) {
        auto& model = models[index];
        unique(model.support_sets);
        model.tally.support_counts.resize(model.support_sets.size());
        transcript_models[model.record.transcript_string].push_back(index);
        for (std::size_t pool = 0; pool < 8; ++pool) {
            const auto policy = pool < 4 ? model.record.exon_policy : model.record.body_policy;
            auto [found, inserted] = winner_index.try_emplace(
                WinnerKey{model.record.transcript_string, policy, pool}, winners.size());
            if (inserted) winners.emplace_back();
            model.winners[pool] = found->second;
        }
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
        }
        return &found->second;
    };
    const auto apply_edge = [&](const EdgeEffects* effects, bool push) {
        if (!effects) return;
        for (const auto& effect : *effects) {
            auto& tally = models[effect.model].tally;
            if (push) { tally.wrong_forward += effect.forward; tally.wrong_reverse += effect.reverse; }
            else { tally.wrong_forward -= effect.forward; tally.wrong_reverse -= effect.reverse; }
        }
    };
    std::vector<Winner> rejected_winners(winners.size());
    const auto update_winner = [&](const Model& model, std::size_t pool, std::int64_t score,
                                   bool rejected) {
        auto& winner = (rejected ? rejected_winners : winners)[model.winners[pool]];
        if (score < winner.score) return;
        if (score > winner.score) { winner.score = score; winner.support_sets.clear(); }
        if (pool / 2 == 2) for (std::size_t i = 0; i < model.support_sets.size(); ++i)
            if (model.tally.support_counts[i] &&
                std::find(winner.support_sets.begin(), winner.support_sets.end(), model.support_sets[i]) == winner.support_sets.end())
                winner.support_sets.push_back(model.support_sets[i]);
    };
    std::uint64_t alignments_scored = 0, piece_visits = 0, model_visits = 0;
    for (const auto* alignment : records) {
        const auto size = static_cast<std::size_t>(alignment->subpath_size());
        if (!size) continue;
        const auto offsets = pathtally::subpath_read_offsets(*alignment);
        std::vector<Piece> pieces(size);
        std::vector<bool> incoming(size, false);
        for (std::size_t index = 0; index < size; ++index) {
            const auto& subpath = alignment->subpath(static_cast<int>(index));
            auto& piece = pieces[index];
            std::vector<std::int64_t> scores;
            pathtally::flat_scorer()(*alignment, static_cast<int>(index), offsets[index], scores);
            std::map<std::uint32_t, Delta> deltas;
            for (int m = 0; m < subpath.path().mapping_size(); ++m) {
                const auto& mapping = subpath.path().mapping(m);
                const auto id = static_cast<std::uint64_t>(mapping.position().node_id());
                const auto& node = nodes.at(id);
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
                for (const auto& posting : node.postings) {
                    if (!(posting.flags & (exon_flags | body_flags))) continue;
                    auto& delta = deltas[posting.model];
                    delta.model = posting.model;
                    delta.score = pathtally::route_checked_add(delta.score, scores[m]);
                    const bool exon = (posting.flags & exon_flags) != 0;
                    const bool body = (posting.flags & body_flags) != 0;
                    if (exon) delta.exon += bases;
                    if (body) {
                        delta.body += bases;
                        delta.gene_score = pathtally::route_checked_add(delta.gene_score, scores[m]);
                        const auto& support = models[posting.model].support_sets;
                        delta.support_slots.push_back(std::lower_bound(support.begin(), support.end(), posting.support_set) - support.begin());
                    }
                    const auto flags = posting.flags & (exon ? exon_flags : body_flags);
                    if (flags & (reverse ? 10U : 5U)) delta.forward += bases;
                    else delta.reverse += bases;
                }
            }
            for (auto& [id, delta] : deltas) {
                unique(delta.support_slots);
                piece.deltas.push_back(std::move(delta));
            }
            const auto mark = [&](std::uint32_t next) {
                if (next >= size || next <= index) throw std::runtime_error("GAMP subpaths are not topologically ordered");
                incoming[next] = true;
            };
            for (const auto next : subpath.next()) mark(next);
            for (const auto& connection : subpath.connection()) mark(connection.next());
        }
        const auto apply_piece = [&](const Piece& piece, bool push) {
            for (const auto& delta : piece.deltas) {
                auto& tally = models[delta.model].tally;
                if (push) {
                    tally.score = pathtally::route_checked_add(tally.score, delta.score);
                    tally.gene_score = pathtally::route_checked_add(tally.gene_score, delta.gene_score);
                    tally.exon += delta.exon; tally.body += delta.body;
                    tally.forward += delta.forward; tally.reverse += delta.reverse;
                    for (const auto slot : delta.support_slots) ++tally.support_counts[slot];
                } else {
                    tally.score -= delta.score; tally.gene_score -= delta.gene_score;
                    tally.exon -= delta.exon; tally.body -= delta.body;
                    tally.forward -= delta.forward; tally.reverse -= delta.reverse;
                    for (const auto slot : delta.support_slots) --tally.support_counts[slot];
                }
            }
            for (const auto* effects : piece.internal_edges) apply_edge(effects, push);
        };
        struct Frame {
            std::uint32_t subpath;
            std::size_t next = 0;
            std::optional<Endpoint> previous;
            const EdgeEffects* entry = nullptr;
        };
        std::vector<Frame> stack;
        stack.reserve(size);
        std::optional<Endpoint> last;
        std::uint64_t total_bases = 0;
        const auto enter = [&](std::uint32_t subpath) {
            if (subpath >= size) throw std::runtime_error("GAMP start index is out of range");
            const auto& piece = pieces[subpath];
            const auto* entry = last && piece.first ? edge_effects(*last, *piece.first) : nullptr;
            stack.push_back({subpath, 0, last, entry});
            apply_edge(entry, true);
            apply_piece(piece, true);
            total_bases += piece.bases;
            if (piece.last) last = piece.last;
            ++piece_visits; model_visits += piece.deltas.size();
        };
        std::vector<std::uint32_t> starts(alignment->start().begin(), alignment->start().end());
        if (starts.empty()) for (std::uint32_t index = 0; index < size; ++index)
            if (!incoming[index]) starts.push_back(index);
        for (const auto start : starts) {
            enter(start);
            while (!stack.empty()) {
                auto& frame = stack.back();
                const auto& subpath = alignment->subpath(frame.subpath);
                const auto next_size = static_cast<std::size_t>(subpath.next_size());
                const auto outgoing = next_size + subpath.connection_size();
                if (!outgoing) {
                    ++alignments_scored;
                    if (total_bases) for (const auto& model : models) {
                        const auto& tally = model.tally;
                        if (!tally.exon && !tally.body) continue;
                        const std::size_t strand = tally.forward >= tally.reverse ? 0 : 1;
                        const bool rejected = strand == 0 ? tally.wrong_forward : tally.wrong_reverse;
                        const std::size_t tier = tally.exon == total_bases ? 0 :
                            (tally.exon >= total_bases - total_bases / 2 ? 1 : 2);
                        if (tier != 2 || tally.body) update_winner(model, 2 * tier + strand, tally.score, rejected);
                        if (tally.body) update_winner(model, 6 + strand, tally.gene_score, rejected);
                    }
                }
                if (frame.next < outgoing) {
                    const auto child = frame.next < next_size ? subpath.next(frame.next) :
                        subpath.connection(frame.next - next_size).next();
                    ++frame.next;
                    enter(child);
                    continue;
                }
                const auto& piece = pieces[frame.subpath];
                apply_piece(piece, false); apply_edge(frame.entry, false);
                total_bases -= piece.bases; last = frame.previous;
                stack.pop_back();
            }
        }
    }
    std::vector<ResolvedAssignment> result(profiles.size());
    std::vector<AssignmentFacts> rejected_facts(profiles.size());
    for (int pass = 0; pass < 2; ++pass) {
    const auto& source_winners = pass ? rejected_winners : winners;
    std::array<std::int64_t, 8> tops;
    tops.fill(no_score);
    std::map<std::pair<std::uint32_t, std::size_t>, std::int64_t> transcript_tops;
    for (const auto& [key, index] : winner_index) {
        auto [found, inserted] = transcript_tops.try_emplace(
            std::pair(std::get<0>(key), std::get<2>(key)), source_winners[index].score);
        if (!inserted) found->second = std::max(found->second, source_winners[index].score);
        tops[std::get<2>(key)] = std::max(tops[std::get<2>(key)], source_winners[index].score);
    }
    for (const auto& [key, index] : winner_index) {
        const auto& winner = source_winners[index];
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
            auto candidate = reference_assignment_candidate(reference_, profiles[profile], policy,
                winner.score, tier, strand, supports);
            auto& facts = pass ? rejected_facts[profile] : result[profile].facts;
            auto& destination = pool < 6 ? facts.exact : facts.gene_fallback;
            destination.push_back(std::move(candidate));
        }
    }
    }
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        result[i].result = resolve_assignment(profiles[i], result[i].facts, AssignmentAlgorithm::node_score_v1);
        if (result[i].facts.exact.empty() && !rejected_facts[i].exact.empty()) {
            // Preserve the barcode/UMI-stage reachability of a feature-bearing
            // read rejected solely by splice direction, never its gene call.
            auto shadow = result[i].facts;
            shadow.exact = std::move(rejected_facts[i].exact);
            shadow.gene_fallback.insert(shadow.gene_fallback.end(),
                rejected_facts[i].gene_fallback.begin(), rejected_facts[i].gene_fallback.end());
            auto assignment = resolve_assignment(profiles[i], shadow, AssignmentAlgorithm::node_score_v1);
            if (assignment.terminal == AssignmentTerminal::assigned_unique ||
                assignment.terminal == AssignmentTerminal::assigned_multigene) {
                assignment.genes.clear();
                assignment.terminal = AssignmentTerminal::unassigned_opposite_strand;
                assignment.reasons |= reason_opposite_strand_veto;
            }
            result[i].result = std::move(assignment);
        }
    }
    static_cast<void>(alignments_scored);
    static_cast<void>(piece_visits);
    static_cast<void>(model_visits);
    return result;
}
}  // namespace pancollapse
