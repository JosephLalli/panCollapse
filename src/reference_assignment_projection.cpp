#include "reference_assignment_projection.hpp"

#include "assignment_engine.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace pancollapse {
namespace {

using count_reference::missing;

constexpr std::uint32_t kNovelParalog = 1;
constexpr std::uint32_t kStrongLocalSupport = 2;
constexpr std::uint32_t kProtectedHost = 4;

std::uint32_t profile_slot(const NumericAssignmentProfile& profile) {
    switch (profile.source_profile) {
        case direct_count::ProfileId::cr7_v1:
            return 0;
        case direct_count::ProfileId::pansc_strict_v1:
            return 1;
    }
    throw std::runtime_error("numeric assignment profile has invalid policy source");
}

void require_string_id(const count_reference::Source& reference, std::uint32_t id,
                       const char* field) {
    if (id == missing || id >= reference.strings().size()) {
        throw std::runtime_error(std::string("prepared count reference lacks ") + field);
    }
}

void require_gene_id(std::span<const count_reference::GeneRecord> genes, std::uint32_t id,
                     const char* field) {
    if (id == missing || id >= genes.size()) {
        throw std::runtime_error(std::string("prepared count ") + field + " is invalid");
    }
}

}  // namespace

NumericAssignmentProfile compile_numeric_profile(
    const count_reference::Source& reference, const direct_count::EffectiveProfile& profile) {
    const auto& source = profile.profile.assignment;
    const auto& strings = reference.strings();
    if (strings.size() >= direct_count::kNumericAssignmentMissing) {
        throw std::runtime_error("prepared count reference exceeds numeric assignment range");
    }

    // There are only four small requested label vectors.  Scan the reference
    // dictionary once rather than retaining a per-profile string hash table.
    std::vector<std::uint32_t> excluded_categories;
    std::vector<std::uint32_t> post_resolution_fallback_categories;
    std::vector<std::uint32_t> body_support_gene_types;
    std::vector<std::uint32_t> strong_support_clearable_categories;
    for (std::size_t index = 0; index < strings.size(); ++index) {
        const auto id = static_cast<std::uint32_t>(index);
        const std::string& label = strings[index];
        if (std::find(source.excluded_categories.begin(), source.excluded_categories.end(), label) !=
            source.excluded_categories.end()) {
            excluded_categories.push_back(id);
        }
        if (std::find(source.post_resolution_fallback_categories.begin(),
                      source.post_resolution_fallback_categories.end(), label) !=
            source.post_resolution_fallback_categories.end()) {
            post_resolution_fallback_categories.push_back(id);
        }
        if (std::find(source.body_support_gene_types.begin(),
                      source.body_support_gene_types.end(), label) !=
            source.body_support_gene_types.end()) {
            body_support_gene_types.push_back(id);
        }
        if (std::find(source.strong_support_clearable_categories.begin(),
                      source.strong_support_clearable_categories.end(), label) !=
            source.strong_support_clearable_categories.end()) {
            strong_support_clearable_categories.push_back(id);
        }
    }

    NumericAssignmentProfile result;
    result.assignment = direct_count::copy_assignment_policy<std::uint32_t>(
        source, excluded_categories, post_resolution_fallback_categories,
        body_support_gene_types, strong_support_clearable_categories);
    result.source_profile = direct_count::profile_policy_source(profile);
    return result;
}

direct_count::NumericAssignmentCandidate numeric_assignment_candidate(
    const count_reference::Source& reference, const NumericAssignmentProfile& profile,
    std::uint32_t policy_id, std::int64_t score, direct_count::EvidenceTier tier,
    direct_count::EvidenceStrand strand, std::span<const std::uint32_t> body_support) {
    const auto policies = reference.policies();
    const auto genes = reference.genes();
    if (policy_id >= policies.size()) {
        throw std::runtime_error("prepared count policy id is invalid");
    }
    const auto& policy = policies[policy_id];
    require_gene_id(genes, policy.gene, "policy gene");

    std::uint32_t resolved_gene_id = policy.gene;
    const bool novel_paralog = (policy.flags & kNovelParalog) != 0;
    bool force_self_equivalence = false;
    if (novel_paralog) {
        require_gene_id(genes, policy.novel_origin, "novel origin gene");
        switch (profile.assignment.novel_paralog) {
            case direct_count::NovelParalogPolicy::lump:
                resolved_gene_id = policy.novel_origin;
                force_self_equivalence = true;
                break;
            case direct_count::NovelParalogPolicy::separate:
                force_self_equivalence = true;
                break;
            case direct_count::NovelParalogPolicy::ignore:
                break;
        }
    }

    const auto& gene = genes[resolved_gene_id];
    const std::uint32_t slot = profile_slot(profile);
    const std::uint32_t equivalent = force_self_equivalence
        ? resolved_gene_id
        : (gene.equivalence[slot] == missing ? resolved_gene_id : gene.equivalence[slot]);
    require_gene_id(genes, equivalent, "equivalence gene");
    const auto& equivalent_gene = genes[equivalent];

    direct_count::NumericAssignmentCandidate result;
    result.gene = resolved_gene_id;
    result.equivalence_gene = equivalent;
    if (equivalent_gene.nested_host[slot] != missing) {
        require_gene_id(genes, equivalent_gene.nested_host[slot], "nested host");
        result.nested_host = equivalent_gene.nested_host[slot];
    }
    result.score = score;
    result.tier = tier;
    result.strand = strand;
    result.competition =
        static_cast<direct_count::CompetitionClass>(equivalent_gene.competition[slot]);

    if (policy.category_set >= reference.sets().size()) {
        throw std::runtime_error("prepared count category set is invalid");
    }
    for (const std::uint32_t category : reference.sets()[policy.category_set]) {
        require_string_id(reference, category, "category identity");
        result.categories.push_back(category);
    }
    if (equivalent_gene.type_set >= reference.sets().size()) {
        throw std::runtime_error("prepared count gene-type set is invalid");
    }
    for (const std::uint32_t type : reference.sets()[equivalent_gene.type_set]) {
        require_string_id(reference, type, "gene-type identity");
        result.gene_types.push_back(type);
    }
    result.strong_local_support = (policy.flags & kStrongLocalSupport) != 0;
    result.protected_primary_protein_nested_host = (policy.flags & kProtectedHost) != 0;
    result.novel_paralog = novel_paralog;
    if (novel_paralog) {
        result.novel_origin_gene = policy.novel_origin;
    }
    if (tier == direct_count::EvidenceTier::body) {
        result.body_support_units.assign(body_support.begin(), body_support.end());
    }
    result.body_sample_support = static_cast<std::uint32_t>(result.body_support_units.size());
    return result;
}

direct_count::NumericAssignmentResult resolve_numeric_assignment(
    const NumericAssignmentProfile& profile, const direct_count::NumericAssignmentFacts& facts,
    bool already_pruned) {
    return direct_count::resolve_assignment_engine(
        profile.assignment, facts, direct_count::AssignmentAlgorithm::node_score_v1, already_pruned);
}

}  // namespace pancollapse
