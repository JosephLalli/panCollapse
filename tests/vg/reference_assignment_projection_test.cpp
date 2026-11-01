#include "count_reference.hpp"
#include "node_score_test_support.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace pancollapse;
using namespace pancollapse::count_reference;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() try {
    Data data;
    data.content_id = "projection-fixture";
    data.strings = {"A", "B", "HOST", "PRIMARY", "protein_coding"};
    data.sets = {{}, {3}, {4}};
    for (std::uint32_t id = 0; id < 3; ++id) {
        GeneRecord gene;
        gene.id_string = id;
        gene.type_set = 2;
        gene.equivalence[0] = gene.equivalence[1] = id;
        gene.competition[0] = 0;
        gene.competition[1] = 1;
        data.genes.push_back(gene);
    }
    data.genes[0].equivalence[0] = 1;
    data.genes[1].nested_host[0] = 2;
    data.policies = {{0, 1, missing, 0}, {0, 1, 1, 7}};
    const auto root = std::filesystem::temp_directory_path() / "pancollapse-projection-test";
    std::filesystem::create_directories(root);
    const auto path = root / "reference.pcr";
    write(path, data);
    const auto reference = Reference::load(path);
    auto cr7 = direct_count::effective_profile(direct_count::ProfileId::cr7_v1);
    auto pansc = direct_count::effective_profile(direct_count::ProfileId::pansc_strict_v1);
    const std::vector<std::uint32_t> support = {7, 42};
    const auto project = [&](const direct_count::EffectiveProfile& profile,
                              std::uint32_t policy, direct_count::EvidenceTier tier) {
        const auto compiled = compile_numeric_profile(reference, profile);
        const auto numeric = numeric_assignment_candidate(reference, compiled, policy, 37, tier,
            direct_count::EvidenceStrand::reverse, support);
        const auto rendered = test::render_candidate(reference, numeric);
        require(rendered == reference_assignment_candidate(reference, profile, policy, 37, tier,
            direct_count::EvidenceStrand::reverse, support), "numeric versus frozen text projection");
        return rendered;
    };
    auto candidate = project(cr7, 0, direct_count::EvidenceTier::body);
    require(candidate.gene == "A" && candidate.equivalence_gene == "B" &&
            candidate.nested_host == "HOST", "profile-specific identity projection");
    require(candidate.score == 37 && candidate.tier == direct_count::EvidenceTier::body &&
            candidate.strand == direct_count::EvidenceStrand::reverse,
            "scoring evidence survives projection");
    require(candidate.categories == std::vector<std::string>{"PRIMARY"} &&
            candidate.gene_types == std::vector<std::string>{"protein_coding"},
            "numeric category/type dictionaries project exactly");
    require(candidate.body_support_units == std::vector<std::string>{"7", "42"} &&
            candidate.body_sample_support == 2, "body support identities survive");
    require(project(cr7, 0, direct_count::EvidenceTier::exon).body_support_units.empty(),
            "body support is not manufactured for exon evidence");
    candidate = project(pansc, 0, direct_count::EvidenceTier::exon);
    require(candidate.equivalence_gene == "A" && candidate.nested_host.empty() &&
            candidate.competition == direct_count::CompetitionClass::fallback,
            "custom profile uses its own policy slot");
    for (const auto policy : {direct_count::NovelParalogPolicy::lump,
                              direct_count::NovelParalogPolicy::separate,
                              direct_count::NovelParalogPolicy::ignore}) {
        cr7.profile.assignment.novel_paralog = policy;
        candidate = project(cr7, 1, direct_count::EvidenceTier::exon);
        require(candidate.novel_paralog && candidate.novel_origin_gene == "B" &&
                candidate.strong_local_support && candidate.protected_primary_protein_nested_host,
                "novel/strong-support/protected-host flags survive projection");
        require(candidate.gene == (policy == direct_count::NovelParalogPolicy::lump ? "B" : "A"),
                "novel-copy output identity follows frozen policy");
        require(candidate.equivalence_gene == (policy == direct_count::NovelParalogPolicy::separate ? "A" : "B"),
                "novel-copy self-equivalence follows frozen policy");
    }
    bool rejected = false;
    try { static_cast<void>(project(cr7, 99, direct_count::EvidenceTier::exon)); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "invalid policy ID must fail");
    std::filesystem::remove(path);
    std::filesystem::remove(root);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
