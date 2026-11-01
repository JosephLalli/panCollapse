#include "direct_count_assignment.hpp"

#include "assignment_engine.hpp"

namespace pancollapse::direct_count {
namespace {

BasicAssignmentPolicy<std::string> compile_text_policy(const EffectiveProfile& profile) {
    const AssignmentPolicy& source = profile.profile.assignment;
    return copy_assignment_policy<std::string>(
        source, source.excluded_categories, source.post_resolution_fallback_categories,
        source.body_support_gene_types, source.strong_support_clearable_categories);
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
                                    const AssignmentFacts& facts,
                                    AssignmentAlgorithm algorithm) {
    return resolve_assignment_engine(compile_text_policy(effective), facts, algorithm);
}

}  // namespace pancollapse::direct_count
