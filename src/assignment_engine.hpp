#pragma once

#include "direct_count_assignment.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <type_traits>
#include <utility>

namespace pancollapse::direct_count {

template<class Id>
BasicAssignmentPolicy<Id> copy_assignment_policy(
    const AssignmentPolicy& source, const std::vector<Id>& excluded_categories,
    const std::vector<Id>& post_resolution_fallback_categories,
    const std::vector<Id>& body_support_gene_types,
    const std::vector<Id>& strong_support_clearable_categories) {
    BasicAssignmentPolicy<Id> result;
    result.strand = source.strand;
    result.score_window = source.score_window;
    result.membership = source.membership;
    result.novel_paralog = source.novel_paralog;
    result.excluded_categories = excluded_categories;
    result.post_resolution_fallback_categories = post_resolution_fallback_categories;
    result.tagged_last_resort = source.tagged_last_resort;
    result.exact_strand_gene_fallback = source.exact_strand_gene_fallback;
    result.nested_host_rule = source.nested_host_rule;
    result.nested_host_action = source.nested_host_action;
    result.body_support_ratio = source.body_support_ratio;
    result.body_support_gene_types = body_support_gene_types;
    result.strong_local_support_exemption = source.strong_local_support_exemption;
    result.strong_support_clearable_categories = strong_support_clearable_categories;
    result.protect_primary_protein_nested_hosts = source.protect_primary_protein_nested_hosts;
    return result;
}

namespace assignment_detail {

template<class Id>
bool id_missing(const Id& value) {
    if constexpr (std::is_same_v<Id, std::string>) {
        return value.empty();
    } else {
        static_assert(std::is_same_v<Id, std::uint32_t>);
        return value == kNumericAssignmentMissing;
    }
}

// The numeric gene catalog is certified lexical.  Missing must nevertheless
// compare before every real ID, exactly as an empty string does in the text
// oracle's representative/nested-host tie break.
template<class Id>
bool id_less(const Id& left, const Id& right) {
    if constexpr (std::is_same_v<Id, std::string>) {
        return left < right;
    } else {
        static_assert(std::is_same_v<Id, std::uint32_t>);
        if (left == kNumericAssignmentMissing) return right != kNumericAssignmentMissing;
        if (right == kNumericAssignmentMissing) return false;
        return left < right;
    }
}

template<class Id>
struct IdLess {
    bool operator()(const Id& left, const Id& right) const {
        return id_less(left, right);
    }
};

template<class Id>
bool contains(const std::vector<Id>& values, const Id& wanted) {
    return std::find(values.begin(), values.end(), wanted) != values.end();
}

template<class Id>
bool intersects(const std::vector<Id>& left, const std::vector<Id>& right) {
    return std::any_of(left.begin(), left.end(), [&](const Id& value) {
        return contains(right, value);
    });
}

inline int tier_rank(EvidenceTier tier) {
    switch (tier) {
        case EvidenceTier::exon:
            return 0;
        case EvidenceTier::partial_exon:
            return 1;
        case EvidenceTier::body:
            return 2;
        case EvidenceTier::gene:
            return 3;
    }
    return 4;
}

template<class Id>
bool category_excluded(const BasicAssignmentCandidate<Id>& candidate,
                       const BasicAssignmentPolicy<Id>& policy, std::uint64_t& reasons,
                       const std::vector<Id>& relaxed_categories = {}) {
    for (const Id& category : candidate.categories) {
        if (contains(relaxed_categories, category)) {
            continue;
        }
        if (policy.strong_local_support_exemption && candidate.strong_local_support &&
            !(policy.protect_primary_protein_nested_hosts &&
              candidate.protected_primary_protein_nested_host) &&
            contains(policy.strong_support_clearable_categories, category)) {
            reasons |= reason_strong_support_exemption;
            continue;
        }
        if (contains(policy.excluded_categories, category)) {
            return true;
        }
    }
    return false;
}

template<class Id>
std::vector<BasicAssignmentCandidate<Id>> filter_categories(
    std::vector<BasicAssignmentCandidate<Id>> candidates,
    const BasicAssignmentPolicy<Id>& policy, std::uint64_t& reasons,
    const std::vector<Id>& relaxed_categories = {}) {
    const std::size_t before = candidates.size();
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [&](const BasicAssignmentCandidate<Id>& candidate) {
                           return category_excluded(candidate, policy, reasons, relaxed_categories);
                       }),
        candidates.end());
    if (candidates.size() != before) {
        reasons |= reason_category_excluded;
    }
    return candidates;
}

template<class Id>
void apply_novel_paralog_policy(std::vector<BasicAssignmentCandidate<Id>>& candidates,
                                NovelParalogPolicy policy) {
    if (policy == NovelParalogPolicy::ignore) {
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                           [](const BasicAssignmentCandidate<Id>& candidate) {
                               return candidate.novel_paralog;
                           }),
            candidates.end());
    } else if (policy == NovelParalogPolicy::lump) {
        for (BasicAssignmentCandidate<Id>& candidate : candidates) {
            if (candidate.novel_paralog && !id_missing(candidate.novel_origin_gene)) {
                // Competition regroups by equivalence identity, so a lumped
                // identity must also replace a stale per-copy equivalence.
                candidate.gene = candidate.novel_origin_gene;
                candidate.equivalence_gene = candidate.novel_origin_gene;
            }
        }
    } else if (policy == NovelParalogPolicy::separate) {
        for (BasicAssignmentCandidate<Id>& candidate : candidates) {
            if (candidate.novel_paralog) {
                candidate.equivalence_gene = candidate.gene;
            }
        }
    }
}

template<class Id>
struct RankSelection {
    std::vector<BasicAssignmentCandidate<Id>> candidates;
    EvidenceTier tier = EvidenceTier::gene;
    EvidenceStrand strand = EvidenceStrand::forward;
    bool opposite_only = false;
};

template<class Id>
RankSelection<Id> select_rank(const std::vector<BasicAssignmentCandidate<Id>>& candidates,
                               StrandPolicy strand_policy, AssignmentAlgorithm algorithm) {
    RankSelection<Id> result;
    if (candidates.empty()) {
        return result;
    }

    if (strand_policy == StrandPolicy::both) {
        const int best = tier_rank(
            std::min_element(candidates.begin(), candidates.end(),
                             [](const BasicAssignmentCandidate<Id>& left,
                                const BasicAssignmentCandidate<Id>& right) {
                                 return tier_rank(left.tier) < tier_rank(right.tier);
                             })
                ->tier);
        for (const BasicAssignmentCandidate<Id>& candidate : candidates) {
            if (tier_rank(candidate.tier) == best) {
                result.candidates.push_back(candidate);
            }
        }
        result.tier = result.candidates.front().tier;
        result.strand = std::any_of(
                            result.candidates.begin(), result.candidates.end(),
                            [](const BasicAssignmentCandidate<Id>& candidate) {
                                return candidate.strand == EvidenceStrand::forward;
                            })
                            ? EvidenceStrand::forward
                            : EvidenceStrand::reverse;
        return result;
    }

    const EvidenceStrand expected = strand_policy == StrandPolicy::forward
                                        ? EvidenceStrand::forward
                                        : EvidenceStrand::reverse;
    if (algorithm == AssignmentAlgorithm::node_score_v1) {
        int best = 8;
        for (const auto& candidate : candidates) {
            best = std::min(best, 2 * tier_rank(candidate.tier) +
                                      (candidate.strand != expected));
        }
        for (const auto& candidate : candidates) {
            if (2 * tier_rank(candidate.tier) + (candidate.strand != expected) == best) {
                result.candidates.push_back(candidate);
            }
        }
        result.tier = result.candidates.front().tier;
        result.strand = result.candidates.front().strand;
        result.opposite_only = result.strand != expected;
        return result;
    }

    const bool sense_present = std::any_of(
        candidates.begin(), candidates.end(), [&](const BasicAssignmentCandidate<Id>& candidate) {
            return candidate.strand == expected;
        });
    int best = 4;
    for (const auto& candidate : candidates) {
        if (!sense_present || candidate.strand == expected) {
            best = std::min(best, tier_rank(candidate.tier));
        }
    }
    for (const auto& candidate : candidates) {
        if ((!sense_present || candidate.strand == expected) && tier_rank(candidate.tier) == best) {
            result.candidates.push_back(candidate);
        }
    }
    result.tier = result.candidates.front().tier;
    result.strand = result.candidates.front().strand;
    result.opposite_only = !sense_present;
    return result;
}

template<class Id>
struct EquivalenceAggregate {
    BasicAssignmentCandidate<Id> representative;
    Id representative_gene = assignment_missing_id<Id>();
    std::uint32_t representative_support = 0;
    std::set<CompetitionClass> competition_classes;
    std::set<Id, IdLess<Id>> gene_types;
    std::set<Id, IdLess<Id>> body_support_units;
};

template<class Id>
std::uint32_t candidate_body_support(const BasicAssignmentCandidate<Id>& candidate) {
    return candidate.body_support_units.empty()
               ? candidate.body_sample_support
               : static_cast<std::uint32_t>(candidate.body_support_units.size());
}

template<class Id>
std::vector<BasicAssignmentCandidate<Id>> apply_gene_competition(
    std::vector<BasicAssignmentCandidate<Id>> candidates,
    const BasicAssignmentPolicy<Id>& policy, EvidenceTier winning_tier,
    EvidenceStrand winning_strand, std::uint64_t& reasons) {
    std::map<Id, EquivalenceAggregate<Id>, IdLess<Id>> groups;
    for (const BasicAssignmentCandidate<Id>& candidate : candidates) {
        const Id key = id_missing(candidate.equivalence_gene) ? candidate.gene
                                                               : candidate.equivalence_gene;
        if (key != candidate.gene) {
            reasons |= reason_equivalence_collapsed;
        }
        const std::uint32_t support = candidate_body_support(candidate);
        auto [found, inserted] = groups.try_emplace(
            key, EquivalenceAggregate<Id>{candidate, candidate.gene, support, {}, {}, {}});
        EquivalenceAggregate<Id>& aggregate = found->second;
        if (!inserted &&
            (support > aggregate.representative_support ||
             (support == aggregate.representative_support &&
              (id_less(candidate.gene, aggregate.representative_gene) ||
               (candidate.gene == aggregate.representative_gene &&
                id_less(candidate.nested_host, aggregate.representative.nested_host)))))) {
            aggregate.representative = candidate;
            aggregate.representative_gene = candidate.gene;
            aggregate.representative_support = support;
        }
        aggregate.representative.gene = key;
        aggregate.competition_classes.insert(candidate.competition);
        aggregate.gene_types.insert(candidate.gene_types.begin(), candidate.gene_types.end());
        aggregate.body_support_units.insert(candidate.body_support_units.begin(),
                                            candidate.body_support_units.end());
    }

    candidates.clear();
    for (auto& [gene, aggregate] : groups) {
        static_cast<void>(gene);
        if (aggregate.competition_classes.size() == 1) {
            aggregate.representative.competition = *aggregate.competition_classes.begin();
        } else {
            aggregate.representative.competition = CompetitionClass::unknown;
        }
        aggregate.representative.gene_types.assign(aggregate.gene_types.begin(),
                                                    aggregate.gene_types.end());
        aggregate.representative.body_support_units.assign(
            aggregate.body_support_units.begin(), aggregate.body_support_units.end());
        if (!aggregate.body_support_units.empty()) {
            aggregate.representative.body_sample_support =
                static_cast<std::uint32_t>(aggregate.body_support_units.size());
        }
        candidates.push_back(std::move(aggregate.representative));
    }

    const bool primary_exists = std::any_of(
        candidates.begin(), candidates.end(), [](const BasicAssignmentCandidate<Id>& candidate) {
            return candidate.competition == CompetitionClass::primary;
        });
    const std::size_t before_membership = candidates.size();
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [&](const BasicAssignmentCandidate<Id>& candidate) {
                           if (primary_exists && candidate.competition == CompetitionClass::fallback) {
                               return true;
                           }
                           if (policy.membership == MembershipClass::all_annotated) {
                               return false;
                           }
                           return candidate.competition != CompetitionClass::primary;
                       }),
        candidates.end());
    if (candidates.size() != before_membership) {
        reasons |= reason_fallback_suppressed;
    }

    if (policy.nested_host_rule == NestedHostRule::contradiction_only &&
        policy.nested_host_action == NestedHostAction::prefer_independent_protein_child &&
        candidates.size() > 1) {
        std::set<Id, IdLess<Id>> retained_genes;
        for (const BasicAssignmentCandidate<Id>& candidate : candidates) {
            retained_genes.insert(candidate.gene);
        }
        std::vector<BasicAssignmentCandidate<Id>> survivors;
        survivors.reserve(candidates.size());
        for (const BasicAssignmentCandidate<Id>& candidate : candidates) {
            if (id_missing(candidate.nested_host) ||
                !retained_genes.contains(candidate.nested_host)) {
                survivors.push_back(candidate);
            }
        }
        // Fail closed for a nesting cycle: retain the unresolved candidates.
        if (!survivors.empty() && survivors.size() < candidates.size()) {
            candidates = std::move(survivors);
            reasons |= reason_nested_host_preferred;
        }
    }

    const EvidenceStrand required_strand = policy.strand == StrandPolicy::reverse
                                               ? EvidenceStrand::reverse
                                               : EvidenceStrand::forward;
    if (policy.body_support_ratio && candidates.size() > 1 &&
        winning_tier == EvidenceTier::body && winning_strand == required_strand) {
        std::vector<BasicAssignmentCandidate<Id>> ranked = candidates;
        std::sort(ranked.begin(), ranked.end(),
                  [](const BasicAssignmentCandidate<Id>& left,
                     const BasicAssignmentCandidate<Id>& right) {
                      return left.body_sample_support != right.body_sample_support
                                 ? left.body_sample_support > right.body_sample_support
                                 : id_less(left.gene, right.gene);
                  });
        const BasicAssignmentCandidate<Id>& leader = ranked.front();
        const std::uint32_t runner_support = ranked[1].body_sample_support;
        const bool leader_type_allowed =
            intersects(leader.gene_types, policy.body_support_gene_types);
        if (leader_type_allowed && runner_support > 0 &&
            leader.body_sample_support > runner_support &&
            static_cast<double>(leader.body_sample_support) >=
                *policy.body_support_ratio * static_cast<double>(runner_support)) {
            candidates = {leader};
            reasons |= reason_body_support_preferred;
        }
    }
    return candidates;
}

template<class Id>
std::vector<BasicAssignmentCandidate<Id>> score_window(
    std::vector<BasicAssignmentCandidate<Id>> candidates, std::int64_t window,
    std::uint64_t& reasons) {
    if (candidates.empty()) {
        return candidates;
    }
    const std::int64_t top =
        std::max_element(candidates.begin(), candidates.end(),
                         [](const BasicAssignmentCandidate<Id>& left,
                            const BasicAssignmentCandidate<Id>& right) {
                             return left.score < right.score;
                         })
            ->score;
    const std::size_t before = candidates.size();
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [&](const BasicAssignmentCandidate<Id>& candidate) {
                           return candidate.score < top - window;
                       }),
        candidates.end());
    if (candidates.size() != before) {
        reasons |= reason_score_window_pruned;
    }
    return candidates;
}

}  // namespace assignment_detail

template<class Id>
BasicAssignmentResult<Id> resolve_assignment_engine(
    const BasicAssignmentPolicy<Id>& policy, const BasicAssignmentFacts<Id>& facts,
    AssignmentAlgorithm algorithm, bool already_pruned = false) {
    using namespace assignment_detail;
    BasicAssignmentResult<Id> result;
    if (!facts.has_complete_provenance) {
        result.reasons |= reason_unavailable_fact;
        result.terminal = AssignmentTerminal::incomplete_facts;
        return result;
    }
    if (facts.exact.empty()) {
        result.reasons |= reason_no_exact_evidence;
        return result;
    }
    result.barcode_correction_eligible = true;

    std::vector<BasicAssignmentCandidate<Id>> unfiltered;
    if (algorithm == AssignmentAlgorithm::legacy) {
        unfiltered = score_window(facts.exact, policy.score_window, result.reasons);
    } else if (already_pruned) {
        // NodeScoreResolver has already applied its six E/P/B-by-strand
        // windows. Only that native producer may request this bypass.
        unfiltered = facts.exact;
    } else {
        for (const auto tier : {EvidenceTier::exon, EvidenceTier::partial_exon,
                                EvidenceTier::body}) {
            for (const auto strand : {EvidenceStrand::forward, EvidenceStrand::reverse}) {
                std::vector<BasicAssignmentCandidate<Id>> pool;
                for (const auto& candidate : facts.exact) {
                    if (candidate.tier == tier && candidate.strand == strand) {
                        pool.push_back(candidate);
                    }
                }
                pool = score_window(std::move(pool), 5, result.reasons);
                unfiltered.insert(unfiltered.end(), pool.begin(), pool.end());
            }
        }
    }
    apply_novel_paralog_policy(unfiltered, policy.novel_paralog);
    std::vector<BasicAssignmentCandidate<Id>> rows =
        filter_categories(unfiltered, policy, result.reasons);
    if (rows.empty() && policy.tagged_last_resort && !unfiltered.empty()) {
        rows = unfiltered;
        result.reasons |= reason_tagged_last_resort;
    }

    RankSelection<Id> selected = select_rank(rows, policy.strand, algorithm);
    result.winning_tier = selected.tier;
    result.winning_strand = selected.strand;
    if (selected.opposite_only) {
        result.reasons |= reason_opposite_strand_veto;
    }

    const auto remove_missing_identity = [&](std::vector<BasicAssignmentCandidate<Id>>& candidates) {
        const std::size_t before = candidates.size();
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                           [](const BasicAssignmentCandidate<Id>& candidate) {
                               return !candidate.identity_known || id_missing(candidate.gene);
                           }),
            candidates.end());
        if (candidates.size() != before) {
            result.reasons |= reason_missing_identity;
        }
    };
    remove_missing_identity(selected.candidates);

    if ((selected.candidates.empty() || selected.opposite_only) &&
        !policy.post_resolution_fallback_categories.empty()) {
        result.reasons |= reason_no_source_retry;
        std::vector<BasicAssignmentCandidate<Id>> retry = filter_categories(
            unfiltered, policy, result.reasons, policy.post_resolution_fallback_categories);
        if (retry.empty() && policy.tagged_last_resort && !unfiltered.empty()) {
            retry = unfiltered;
            result.reasons |= reason_tagged_last_resort;
        }
        RankSelection<Id> retried = select_rank(retry, policy.strand, algorithm);
        remove_missing_identity(retried.candidates);
        selected = std::move(retried);
        result.winning_tier = selected.tier;
        result.winning_strand = selected.strand;
    }

    if (selected.opposite_only && policy.exact_strand_gene_fallback) {
        std::vector<BasicAssignmentCandidate<Id>> sense_gene_rows = facts.gene_fallback;
        const EvidenceStrand expected = policy.strand == StrandPolicy::reverse
                                            ? EvidenceStrand::reverse
                                            : EvidenceStrand::forward;
        sense_gene_rows.erase(
            std::remove_if(sense_gene_rows.begin(), sense_gene_rows.end(),
                           [&](const BasicAssignmentCandidate<Id>& candidate) {
                               return candidate.strand != expected;
                           }),
            sense_gene_rows.end());
        apply_novel_paralog_policy(sense_gene_rows, policy.novel_paralog);
        std::vector<BasicAssignmentCandidate<Id>> fallback =
            filter_categories(sense_gene_rows, policy, result.reasons);
        if (fallback.empty() && !sense_gene_rows.empty()) {
            fallback = sense_gene_rows;
            result.reasons |= reason_tagged_last_resort;
        }
        remove_missing_identity(fallback);
        if (!fallback.empty()) {
            selected.candidates = std::move(fallback);
            selected.opposite_only = false;
            result.winning_tier = EvidenceTier::gene;
            result.winning_strand = expected;
            result.reasons |= reason_gene_fallback;
        }
    }
    if (selected.opposite_only) {
        result.terminal = AssignmentTerminal::unassigned_opposite_strand;
        return result;
    }

    if (selected.candidates.empty()) {
        result.terminal = (result.reasons & reason_missing_identity) != 0
                              ? AssignmentTerminal::unassigned_no_identity
                              : AssignmentTerminal::unassigned_no_evidence;
        result.reaches_umi_filter = (result.reasons & reason_missing_identity) != 0;
        return result;
    }

    result.reaches_umi_filter = true;
    selected.candidates = apply_gene_competition(
        std::move(selected.candidates), policy, result.winning_tier,
        result.winning_strand, result.reasons);
    for (const auto& candidate : selected.candidates) {
        if (!id_missing(candidate.gene)) {
            result.genes.push_back(candidate.gene);
        }
    }
    std::sort(result.genes.begin(), result.genes.end(), assignment_detail::IdLess<Id>{});
    result.genes.erase(std::unique(result.genes.begin(), result.genes.end()), result.genes.end());
    if (result.genes.empty()) {
        result.terminal = AssignmentTerminal::unassigned_no_identity;
    } else if (result.genes.size() == 1) {
        result.terminal = AssignmentTerminal::assigned_unique;
    } else {
        result.terminal = AssignmentTerminal::assigned_multigene;
    }
    return result;
}

}  // namespace pancollapse::direct_count
