#pragma once

#include "direct_count_facts.hpp"

#include <algorithm>
#include <tuple>

namespace pancollapse::direct_count {

// All varying arguments to CountFactCatalog::candidate. Use only within one
// immutable catalog/profile context, with complete provenance. Lexical Parent
// identity is the cache key; numeric rank only locates its compact policy row.
struct AssignmentCandidateInput {
    CompactIdentityText unique_parent;
    std::int64_t score;
    EvidenceTier tier;
    EvidenceStrand strand;
    std::uint32_t body_sample_support = 0;
    std::uint32_t parent_rank = 0;
};

// Digest supplies length-framed string(), fixed-width integer(), and byte().
// Sort references only: materialization order and duplicate multiplicity survive.
template<class Digest>
void digest_assignment_inputs(
    Digest& digest, const std::vector<AssignmentCandidateInput>& inputs,
    std::vector<const AssignmentCandidateInput*>& rows) {
    rows.clear();
    rows.reserve(inputs.size());
    for (const auto& input : inputs) rows.push_back(&input);
    const auto key = [](const AssignmentCandidateInput& input) {
        return std::tuple(std::string_view(input.unique_parent), input.score, input.tier,
                          input.strand, input.body_sample_support);
    };
    std::sort(rows.begin(), rows.end(), [&](const auto* left, const auto* right) {
        return key(*left) < key(*right);
    });
    digest.integer(rows.size());
    for (const auto* input : rows) {
        digest.string(input->unique_parent);
        digest.integer(static_cast<std::uint64_t>(input->score));
        digest.byte(static_cast<std::uint8_t>(input->tier));
        digest.byte(static_cast<std::uint8_t>(input->strand));
        digest.integer(input->body_sample_support);
    }
}

}  // namespace pancollapse::direct_count
