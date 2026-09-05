#include "direct_count_assignment.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace pancollapse::direct_count {
namespace {

int tier_rank(EvidenceTier tier) {
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

bool contains(const std::vector<std::string>& values, std::string_view wanted) {
    return std::find(values.begin(), values.end(), wanted) != values.end();
}

bool intersects(const std::vector<std::string>& left,
                const std::vector<std::string>& right) {
    return std::any_of(left.begin(), left.end(),
                       [&](const std::string& value) { return contains(right, value); });
}

bool category_excluded(const AssignmentCandidate& candidate,
                       const AssignmentPolicy& policy, std::uint64_t& reasons,
                       const std::vector<std::string>& relaxed_categories = {}) {
    for (const std::string& category : candidate.categories) {
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

std::vector<AssignmentCandidate>
filter_categories(std::vector<AssignmentCandidate> candidates,
                  const AssignmentPolicy& policy, std::uint64_t& reasons,
                  const std::vector<std::string>& relaxed_categories = {}) {
    const size_t before = candidates.size();
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [&](const AssignmentCandidate& candidate) {
                           return category_excluded(candidate, policy, reasons,
                                                    relaxed_categories);
                       }),
        candidates.end());
    if (candidates.size() != before) {
        reasons |= reason_category_excluded;
    }
    return candidates;
}

void apply_novel_paralog_policy(std::vector<AssignmentCandidate>& candidates,
                                NovelParalogPolicy policy) {
    if (policy == NovelParalogPolicy::ignore) {
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                           [](const AssignmentCandidate& candidate) {
                               return candidate.novel_paralog;
                           }),
            candidates.end());
    } else if (policy == NovelParalogPolicy::lump) {
        for (AssignmentCandidate& candidate : candidates) {
            if (candidate.novel_paralog && !candidate.novel_origin_gene.empty()) {
                // Gene competition regroups by equivalence_gene, so the lumped
                // identity must also become the equivalence key; otherwise a
                // per-paralog equivalence row silently restores the paralog.
                candidate.gene = candidate.novel_origin_gene;
                candidate.equivalence_gene = candidate.novel_origin_gene;
            }
        }
    } else if (policy == NovelParalogPolicy::separate) {
        for (AssignmentCandidate& candidate : candidates) {
            if (candidate.novel_paralog) {
                // A bundle may map a paralog's equivalence to its origin gene;
                // separate keeps the paralog distinct under its own identity.
                candidate.equivalence_gene = candidate.gene;
            }
        }
    }
}

struct RankSelection {
    std::vector<AssignmentCandidate> candidates;
    EvidenceTier tier = EvidenceTier::gene;
    EvidenceStrand strand = EvidenceStrand::forward;
    bool opposite_only = false;
};

RankSelection select_rank(const std::vector<AssignmentCandidate>& candidates,
                          StrandPolicy strand_policy) {
    RankSelection result;
    if (candidates.empty()) {
        return result;
    }

    if (strand_policy == StrandPolicy::both) {
        const int best = tier_rank(
            std::min_element(candidates.begin(), candidates.end(),
                             [](const AssignmentCandidate& left,
                                const AssignmentCandidate& right) {
                                 return tier_rank(left.tier) < tier_rank(right.tier);
                             })
                ->tier);
        for (const AssignmentCandidate& candidate : candidates) {
            if (tier_rank(candidate.tier) == best) {
                result.candidates.push_back(candidate);
            }
        }
        result.tier = result.candidates.front().tier;
        // Candidate order follows evidence-set iteration, which the assignment
        // cache key deliberately ignores. Derive the winning strand from the
        // selected strand set rather than from whichever candidate is first:
        // library sense wins whenever any best-tier candidate carries it.
        result.strand = std::any_of(result.candidates.begin(), result.candidates.end(),
                                    [](const AssignmentCandidate& candidate) {
                                        return candidate.strand == EvidenceStrand::forward;
                                    })
                            ? EvidenceStrand::forward
                            : EvidenceStrand::reverse;
        return result;
    }

    const EvidenceStrand expected = strand_policy == StrandPolicy::forward
                                        ? EvidenceStrand::forward
                                        : EvidenceStrand::reverse;
    const bool sense_present = std::any_of(
        candidates.begin(), candidates.end(), [&](const AssignmentCandidate& candidate) {
            return candidate.strand == expected;
        });
    int best = 4;
    for (const AssignmentCandidate& candidate : candidates) {
        if (!sense_present || candidate.strand == expected) {
            best = std::min(best, tier_rank(candidate.tier));
        }
    }
    for (const AssignmentCandidate& candidate : candidates) {
        if ((!sense_present || candidate.strand == expected) &&
            tier_rank(candidate.tier) == best) {
            result.candidates.push_back(candidate);
        }
    }
    result.tier = result.candidates.front().tier;
    result.strand = result.candidates.front().strand;
    result.opposite_only = !sense_present;
    return result;
}

struct EquivalenceAggregate {
    AssignmentCandidate representative;
    std::string representative_gene;
    std::uint32_t representative_support = 0;
    std::set<CompetitionClass> competition_classes;
    std::set<std::string> gene_types;
    std::set<std::string> body_support_units;
};

// Production candidates carry sample units; compact fixtures carry the count.
std::uint32_t candidate_body_support(const AssignmentCandidate& candidate) {
    return candidate.body_support_units.empty()
               ? candidate.body_sample_support
               : static_cast<std::uint32_t>(candidate.body_support_units.size());
}

std::vector<AssignmentCandidate>
apply_gene_competition(std::vector<AssignmentCandidate> candidates,
                       const AssignmentPolicy& policy, EvidenceTier winning_tier,
                       EvidenceStrand winning_strand, std::uint64_t& reasons) {
    std::map<std::string, EquivalenceAggregate> groups;
    for (const AssignmentCandidate& candidate : candidates) {
        const std::string key = candidate.equivalence_gene.empty()
                                    ? candidate.gene
                                    : candidate.equivalence_gene;
        if (key != candidate.gene) {
            reasons |= reason_equivalence_collapsed;
        }
        const std::uint32_t support = candidate_body_support(candidate);
        auto [found, inserted] = groups.try_emplace(
            key, EquivalenceAggregate{candidate, candidate.gene, support, {}, {}, {}});
        EquivalenceAggregate& aggregate = found->second;
        // The representative supplies every per-gene field that is not unioned
        // below (nested_host in particular). Choose it by body support, then by
        // gene identity, so the choice does not depend on candidate order.
        if (!inserted && (support > aggregate.representative_support ||
                          (support == aggregate.representative_support &&
                           candidate.gene < aggregate.representative_gene))) {
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
        candidates.begin(), candidates.end(), [](const AssignmentCandidate& candidate) {
            return candidate.competition == CompetitionClass::primary;
        });
    const size_t before_membership = candidates.size();
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [&](const AssignmentCandidate& candidate) {
                           if (primary_exists &&
                               candidate.competition == CompetitionClass::fallback) {
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
        policy.nested_host_action ==
            NestedHostAction::prefer_independent_protein_child &&
        candidates.size() > 1) {
        // The projected policy is already directional: the row carrying
        // nested_host is the candidate that loses when its named counterpart
        // co-occurs. In particular, the frozen positive-child derivation reverses
        // a contradicted child->host relation by writing host.nested_host=child.
        // Do not reinterpret the relation here or the selected child is lost.
        const std::set<std::string> retained_genes = [&] {
            std::set<std::string> genes;
            for (const AssignmentCandidate& candidate : candidates) {
                genes.insert(candidate.gene);
            }
            return genes;
        }();
        std::vector<AssignmentCandidate> survivors;
        survivors.reserve(candidates.size());
        for (const AssignmentCandidate& candidate : candidates) {
            if (candidate.nested_host.empty() ||
                !retained_genes.contains(candidate.nested_host)) {
                survivors.push_back(candidate);
            }
        }
        // Match the frozen oracle's fail-closed cycle handling: if every
        // candidate points at another retained candidate, keep the unresolved
        // set rather than deleting the entire assignment.
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
        std::vector<AssignmentCandidate> ranked = candidates;
        std::sort(ranked.begin(), ranked.end(),
                  [](const AssignmentCandidate& left,
                     const AssignmentCandidate& right) {
                      return left.body_sample_support != right.body_sample_support
                                 ? left.body_sample_support > right.body_sample_support
                                 : left.gene < right.gene;
                  });
        const AssignmentCandidate& leader = ranked.front();
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

std::vector<AssignmentCandidate>
score_window(std::vector<AssignmentCandidate> candidates, std::int64_t window,
             std::uint64_t& reasons) {
    if (candidates.empty()) {
        return candidates;
    }
    const std::int64_t top =
        std::max_element(candidates.begin(), candidates.end(),
                         [](const AssignmentCandidate& left,
                            const AssignmentCandidate& right) {
                             return left.score < right.score;
                         })
            ->score;
    const size_t before = candidates.size();
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [&](const AssignmentCandidate& candidate) {
                           return static_cast<std::int64_t>(candidate.score) <
                                  static_cast<std::int64_t>(top) - window;
                       }),
        candidates.end());
    if (candidates.size() != before) {
        reasons |= reason_score_window_pruned;
    }
    return candidates;
}

}  // namespace

CountTerminal terminal_after_umi_filter(const AssignmentResult& assignment,
                                        BasicUmiStatus umi_status) {
    CountTerminal result{
        std::string(assignment_terminal_name(assignment.terminal)), assignment.reasons};
    if (!assignment.reaches_umi_filter) {
        return result;
    }
    switch (umi_status) {
        case BasicUmiStatus::valid:
            break;
        case BasicUmiStatus::contains_n:
            result.name = "umi_n";
            result.reasons |= reason_umi_n;
            break;
        case BasicUmiStatus::homopolymer:
            result.name = "umi_homopolymer";
            result.reasons |= reason_umi_homopolymer;
            break;
        case BasicUmiStatus::empty:
            result.name = "invalid_umi";
            break;
    }
    return result;
}

AssignmentResult resolve_assignment(const EffectiveProfile& effective,
                                    const AssignmentFacts& facts) {
    AssignmentResult result;
    const AssignmentPolicy& policy = effective.profile.assignment;
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

    std::vector<AssignmentCandidate> unfiltered =
        score_window(facts.exact, policy.score_window, result.reasons);
    apply_novel_paralog_policy(unfiltered, policy.novel_paralog);
    std::vector<AssignmentCandidate> rows =
        filter_categories(unfiltered, policy, result.reasons);
    if (rows.empty() && policy.tagged_last_resort && !unfiltered.empty()) {
        rows = unfiltered;
        result.reasons |= reason_tagged_last_resort;
    }

    RankSelection selected = select_rank(rows, policy.strand);
    result.winning_tier = selected.tier;
    result.winning_strand = selected.strand;
    if (selected.opposite_only) {
        result.reasons |= reason_opposite_strand_veto;
    }

    auto remove_missing_identity = [&](std::vector<AssignmentCandidate>& candidates) {
        const size_t before = candidates.size();
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                           [](const AssignmentCandidate& candidate) {
                               return !candidate.identity_known || candidate.gene.empty();
                           }),
            candidates.end());
        if (candidates.size() != before) {
            result.reasons |= reason_missing_identity;
        }
    };
    remove_missing_identity(selected.candidates);

    // This retry occurs only after ordinary category/rank/identity resolution became
    // empty. It relaxes exactly the frozen fallback categories and re-runs the same
    // rank and identity gates; it never turns a surviving lower-quality call into a
    // post hoc competitor.
    // The oracle resolves this fallback before its opposite-strand veto.  An
    // excluded sense explanation must therefore be allowed to replace an
    // initial antisense-only winner before considering exact-Gene fallback.
    if ((selected.candidates.empty() || selected.opposite_only) &&
        !policy.post_resolution_fallback_categories.empty()) {
        result.reasons |= reason_no_source_retry;
        std::vector<AssignmentCandidate> retry = filter_categories(
            unfiltered, policy, result.reasons,
            policy.post_resolution_fallback_categories);
        // The frozen Python retry preserves the profile's all-tagged
        // last-resort contract. In particular, an excluded antisense winner
        // must remain visible as a reverse winner so exact-strand Gene
        // fallback can replace it with a sense G-row explanation.
        if (retry.empty() && policy.tagged_last_resort && !unfiltered.empty()) {
            retry = unfiltered;
            result.reasons |= reason_tagged_last_resort;
        }
        RankSelection retried = select_rank(retry, policy.strand);
        remove_missing_identity(retried.candidates);
        selected = std::move(retried);
        result.winning_tier = selected.tier;
        result.winning_strand = selected.strand;
    }

    if (selected.opposite_only && policy.exact_strand_gene_fallback) {
        // `count_cr.py` selects library-sense G rows *before* category
        // preference.  Filtering first lets a clean antisense G row suppress
        // tagged-last-resort recovery of the only sense Gene explanation.
        std::vector<AssignmentCandidate> sense_gene_rows = facts.gene_fallback;
        const EvidenceStrand expected = policy.strand == StrandPolicy::reverse
                                            ? EvidenceStrand::reverse
                                            : EvidenceStrand::forward;
        sense_gene_rows.erase(
            std::remove_if(sense_gene_rows.begin(), sense_gene_rows.end(),
                           [&](const AssignmentCandidate& candidate) {
                               return candidate.strand != expected;
                           }),
            sense_gene_rows.end());
        apply_novel_paralog_policy(sense_gene_rows, policy.novel_paralog);
        std::vector<AssignmentCandidate> fallback =
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
        // A structurally valid exact row whose certified identity disappears
        // reaches count_cr's post-resolution empty-gene state. Category-only,
        // novel-only, and other evidence-policy exits do not.
        result.reaches_umi_filter =
            (result.reasons & reason_missing_identity) != 0;
        return result;
    }

    result.reaches_umi_filter = true;
    selected.candidates = apply_gene_competition(
        std::move(selected.candidates), policy, result.winning_tier,
        result.winning_strand, result.reasons);
    for (const AssignmentCandidate& candidate : selected.candidates) {
        if (!candidate.gene.empty()) {
            result.genes.push_back(candidate.gene);
        }
    }
    std::sort(result.genes.begin(), result.genes.end());
    result.genes.erase(std::unique(result.genes.begin(), result.genes.end()),
                       result.genes.end());
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
