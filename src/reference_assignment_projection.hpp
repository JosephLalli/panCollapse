#pragma once

#include "count_reference.hpp"
#include "direct_count_assignment.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace pancollapse {

// Compiled once per selected effective profile.  Labels are reference-string
// IDs, so per-read projection and resolution never materialize string labels.
struct NumericAssignmentProfile {
    direct_count::BasicAssignmentPolicy<std::uint32_t> assignment;
    direct_count::ProfileId source_profile = direct_count::ProfileId::cr7_v1;
};

struct NumericResolvedAssignment {
    direct_count::NumericAssignmentFacts facts;
    direct_count::NumericAssignmentResult result;
};

NumericAssignmentProfile compile_numeric_profile(
    const count_reference::Source& reference, const direct_count::EffectiveProfile& profile);

direct_count::NumericAssignmentCandidate numeric_assignment_candidate(
    const count_reference::Source& reference, const NumericAssignmentProfile& profile,
    std::uint32_t policy_id, std::int64_t score, direct_count::EvidenceTier tier,
    direct_count::EvidenceStrand strand, std::span<const std::uint32_t> body_support);

direct_count::NumericAssignmentResult resolve_numeric_assignment(
    const NumericAssignmentProfile& profile, const direct_count::NumericAssignmentFacts& facts,
    bool already_pruned = false);

}  // namespace pancollapse
