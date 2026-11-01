#pragma once

// Text rendering belongs to test diagnostics, never the production count path.
#include "node_score_resolver.hpp"

#include <stdexcept>

namespace pancollapse {

struct ResolvedAssignment {
    direct_count::AssignmentFacts facts;
    direct_count::AssignmentResult result;
};

direct_count::AssignmentCandidate reference_assignment_candidate(
    const count_reference::Source&, const direct_count::EffectiveProfile&,
    std::uint32_t, std::int64_t, direct_count::EvidenceTier,
    direct_count::EvidenceStrand, std::span<const std::uint32_t>);

std::vector<ResolvedAssignment> enumerate_node_score(
    const count_reference::Source&, const std::vector<const vg::MultipathAlignment*>&,
    std::span<const direct_count::EffectiveProfile>);

namespace test {

inline std::string gene_text(const count_reference::Source& reference, std::uint32_t id) {
    if (id == count_reference::missing) return {};
    if (id >= reference.genes().size()) throw std::runtime_error("invalid test gene ID");
    return reference.strings().at(reference.genes()[id].id_string);
}

inline direct_count::AssignmentCandidate render_candidate(
    const count_reference::Source& reference,
    const direct_count::NumericAssignmentCandidate& input) {
    direct_count::AssignmentCandidate output;
    output.gene = gene_text(reference, input.gene);
    output.equivalence_gene = gene_text(reference, input.equivalence_gene);
    output.nested_host = gene_text(reference, input.nested_host);
    output.score = input.score;
    output.tier = input.tier;
    output.strand = input.strand;
    output.competition = input.competition;
    for (const auto id : input.categories) output.categories.push_back(reference.strings().at(id));
    for (const auto id : input.gene_types) output.gene_types.push_back(reference.strings().at(id));
    for (const auto id : input.body_support_units) output.body_support_units.push_back(std::to_string(id));
    output.body_sample_support = input.body_sample_support;
    output.identity_known = input.identity_known;
    output.novel_paralog = input.novel_paralog;
    output.novel_origin_gene = gene_text(reference, input.novel_origin_gene);
    output.strong_local_support = input.strong_local_support;
    output.protected_primary_protein_nested_host = input.protected_primary_protein_nested_host;
    return output;
}

inline ResolvedAssignment render_assignment(const count_reference::Source& reference,
                                             const NumericResolvedAssignment& input) {
    ResolvedAssignment output;
    output.facts.has_complete_provenance = input.facts.has_complete_provenance;
    for (const auto& candidate : input.facts.exact)
        output.facts.exact.push_back(render_candidate(reference, candidate));
    for (const auto& candidate : input.facts.gene_fallback)
        output.facts.gene_fallback.push_back(render_candidate(reference, candidate));
    output.result.terminal = input.result.terminal;
    for (const auto id : input.result.genes) output.result.genes.push_back(gene_text(reference, id));
    output.result.reasons = input.result.reasons;
    output.result.winning_tier = input.result.winning_tier;
    output.result.winning_strand = input.result.winning_strand;
    output.result.barcode_correction_eligible = input.result.barcode_correction_eligible;
    output.result.reaches_umi_filter = input.result.reaches_umi_filter;
    return output;
}

class TextNodeScoreResolver {
  public:
    explicit TextNodeScoreResolver(const count_reference::Source& reference,
        NodeScoreExecution execution = NodeScoreExecution::automatic,
        std::size_t frontier_batch = 64)
        : reference_(reference), execution_(execution), scorer_(reference, execution, frontier_batch) {}

    std::vector<ResolvedAssignment> resolve(
        const std::vector<const vg::MultipathAlignment*>& records,
        std::span<const direct_count::EffectiveProfile> profiles) const {
        if (execution_ == NodeScoreExecution::enumerate_oracle)
            return enumerate_node_score(reference_, records, profiles);
        std::vector<NumericAssignmentProfile> compiled;
        for (const auto& profile : profiles) compiled.push_back(compile_numeric_profile(reference_, profile));
        std::vector<ResolvedAssignment> output;
        for (const auto& assignment : scorer_.resolve(records, compiled))
            output.push_back(render_assignment(reference_, assignment));
        return output;
    }
    NodeScoreStats stats() const noexcept { return scorer_.stats(); }

  private:
    const count_reference::Source& reference_;
    NodeScoreExecution execution_;
    pancollapse::NodeScoreResolver scorer_;
};

} // namespace test
} // namespace pancollapse
