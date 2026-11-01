// Frozen text projection for the bounded test oracle, never linked by count.
#include "node_score_test_support.hpp"

#include <stdexcept>
#include <string>

namespace pancollapse {
namespace {

using count_reference::missing;
using direct_count::AssignmentCandidate;
using direct_count::CompetitionClass;
using direct_count::EffectiveProfile;
using direct_count::EvidenceStrand;
using direct_count::EvidenceTier;

constexpr std::uint32_t kNovelParalog = 1;
constexpr std::uint32_t kStrongLocalSupport = 2;
constexpr std::uint32_t kProtectedHost = 4;

std::uint32_t profile_slot(const EffectiveProfile& profile) {
    return direct_count::profile_policy_source(profile) == direct_count::ProfileId::cr7_v1 ? 0 : 1;
}

const std::string& required_string(const count_reference::Source& reference,
                                   std::uint32_t id, const char* field) {
    if (id == missing || id >= reference.strings().size()) {
        throw std::runtime_error(std::string("prepared count reference lacks ") + field);
    }
    return reference.strings()[id];
}

}  // namespace

AssignmentCandidate reference_assignment_candidate(
    const count_reference::Source& reference, const EffectiveProfile& profile,
    std::uint32_t policy_id, std::int64_t score, EvidenceTier tier,
    EvidenceStrand strand, std::span<const std::uint32_t> body_support) {
    const auto policies = reference.policies();
    const auto genes = reference.genes();
    if (policy_id >= policies.size()) throw std::runtime_error("prepared count policy id is invalid");
    const auto& policy = policies[policy_id];
    if (policy.gene >= genes.size()) throw std::runtime_error("prepared count policy gene is invalid");
    std::uint32_t resolved_gene_id = policy.gene;
    const bool novel_paralog = (policy.flags & kNovelParalog) != 0;
    bool force_self_equivalence = false;
    if (novel_paralog) {
        if (policy.novel_origin == missing || policy.novel_origin >= genes.size()) {
            throw std::runtime_error("prepared count novel origin gene is invalid");
        }
        switch (profile.profile.assignment.novel_paralog) {
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
    if (equivalent >= genes.size()) throw std::runtime_error("prepared count equivalence gene is invalid");
    const auto& equivalent_gene = genes[equivalent];

    AssignmentCandidate result;
    result.gene = required_string(reference, gene.id_string, "gene identity");
    result.equivalence_gene = required_string(reference, equivalent_gene.id_string,
                                              "equivalence gene identity");
    if (equivalent_gene.nested_host[slot] != missing) {
        if (equivalent_gene.nested_host[slot] >= genes.size()) {
            throw std::runtime_error("prepared count nested host is invalid");
        }
        result.nested_host = required_string(reference,
                                             genes[equivalent_gene.nested_host[slot]].id_string,
                                             "nested host identity");
    }
    result.score = score;
    result.tier = tier;
    result.strand = strand;
    result.competition = static_cast<CompetitionClass>(equivalent_gene.competition[slot]);
    if (policy.category_set >= reference.sets().size()) {
        throw std::runtime_error("prepared count category set is invalid");
    }
    for (const std::uint32_t category : reference.sets()[policy.category_set]) {
        result.categories.push_back(required_string(reference, category, "category identity"));
    }
    if (equivalent_gene.type_set >= reference.sets().size()) {
        throw std::runtime_error("prepared count gene-type set is invalid");
    }
    for (const std::uint32_t type : reference.sets()[equivalent_gene.type_set]) {
        result.gene_types.push_back(required_string(reference, type, "gene type"));
    }
    result.strong_local_support = (policy.flags & kStrongLocalSupport) != 0;
    result.protected_primary_protein_nested_host = (policy.flags & kProtectedHost) != 0;
    result.novel_paralog = novel_paralog;
    if (novel_paralog) {
        result.novel_origin_gene = required_string(reference, genes[policy.novel_origin].id_string,
                                                   "novel origin gene identity");
    }
    if (tier == EvidenceTier::body) {
        for (const std::uint32_t support : body_support) {
            result.body_support_units.push_back(std::to_string(support));
        }
    }
    result.body_sample_support = static_cast<std::uint32_t>(result.body_support_units.size());
    return result;
}

}  // namespace pancollapse
