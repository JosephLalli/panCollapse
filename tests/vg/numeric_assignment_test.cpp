#include "count_reference.hpp"
#include "direct_count_assignment.hpp"
#include "reference_assignment_projection.hpp"

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace pancollapse;
using namespace pancollapse::count_reference;
using namespace pancollapse::direct_count;

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::uint32_t gene_id(std::string_view gene) {
    if (gene.empty()) return missing;
    if (gene.size() != 1 || gene.front() < 'A' || gene.front() > 'H') {
        throw std::runtime_error("test has unknown gene");
    }
    return static_cast<std::uint32_t>(gene.front() - 'A');
}

std::uint32_t label_id(std::string_view label) {
    if (label == "no_source_provenance") return 8;
    if (label == "protein_coding") return 9;
    if (label == "readthrough_transcript") return 10;
    if (label == "source_gene_label_conflict") return 11;
    throw std::runtime_error("test has unknown label");
}

struct Pair {
    AssignmentCandidate text;
    NumericAssignmentCandidate numeric;
};

Pair candidate(std::string gene, EvidenceTier tier, EvidenceStrand strand,
               std::int64_t score = 100) {
    Pair result;
    result.text.gene = gene;
    result.numeric.gene = gene_id(gene);
    result.text.tier = result.numeric.tier = tier;
    result.text.strand = result.numeric.strand = strand;
    result.text.score = result.numeric.score = score;
    result.text.competition = result.numeric.competition = CompetitionClass::primary;
    result.text.gene_types = {"protein_coding"};
    result.numeric.gene_types = {label_id("protein_coding")};
    return result;
}

void equivalence(Pair& value, std::string id) {
    value.text.equivalence_gene = id;
    value.numeric.equivalence_gene = gene_id(id);
}

void nested_host(Pair& value, std::string id) {
    value.text.nested_host = id;
    value.numeric.nested_host = gene_id(id);
}

void category(Pair& value, std::string_view label) {
    value.text.categories.emplace_back(label);
    value.numeric.categories.push_back(label_id(label));
}

void support(Pair& value, std::initializer_list<std::uint32_t> units) {
    for (const auto unit : units) {
        value.text.body_support_units.push_back("s" + std::to_string(unit));
        value.numeric.body_support_units.push_back(unit);
    }
    value.text.body_sample_support = value.numeric.body_sample_support =
        static_cast<std::uint32_t>(units.size());
}

AssignmentResult render_result(const Source& reference,
                               const NumericAssignmentResult& numeric) {
    AssignmentResult result;
    result.terminal = numeric.terminal;
    for (const auto id : numeric.genes) {
        require(id != missing && id < reference.genes().size(), "numeric result has invalid gene");
        result.genes.push_back(reference.strings().at(reference.genes()[id].id_string));
    }
    result.reasons = numeric.reasons;
    result.winning_tier = numeric.winning_tier;
    result.winning_strand = numeric.winning_strand;
    result.barcode_correction_eligible = numeric.barcode_correction_eligible;
    result.reaches_umi_filter = numeric.reaches_umi_filter;
    return result;
}

AssignmentResult compare_case(const Source& reference, const EffectiveProfile& profile,
                              const NumericAssignmentProfile& numeric_profile,
                              const std::vector<Pair>& exact,
                              const std::vector<Pair>& fallback, bool already_pruned,
                              const char* message) {
    AssignmentFacts text_facts;
    NumericAssignmentFacts numeric_facts;
    for (const auto& value : exact) {
        text_facts.exact.push_back(value.text);
        numeric_facts.exact.push_back(value.numeric);
    }
    for (const auto& value : fallback) {
        text_facts.gene_fallback.push_back(value.text);
        numeric_facts.gene_fallback.push_back(value.numeric);
    }
    const auto expected = resolve_assignment(profile, text_facts, AssignmentAlgorithm::node_score_v1);
    const auto observed = render_result(
        reference, resolve_numeric_assignment(numeric_profile, numeric_facts, already_pruned));
    require(expected.terminal == observed.terminal && expected.genes == observed.genes &&
                expected.reasons == observed.reasons &&
                expected.winning_tier == observed.winning_tier &&
                expected.winning_strand == observed.winning_strand &&
                expected.barcode_correction_eligible == observed.barcode_correction_eligible &&
                expected.reaches_umi_filter == observed.reaches_umi_filter,
            message);
    return observed;
}

}  // namespace

int main() try {
    // Gene record IDs are lexical feature IDs A..H. UINT32_MAX is reserved for
    // missing identity and must sort before those IDs where the text oracle has
    // an empty string.
    Data data;
    data.content_id = "numeric-assignment-fixture";
    data.strings = {"A", "B", "C", "D", "E", "F", "G", "H",
                    "no_source_provenance", "protein_coding", "readthrough_transcript",
                    "source_gene_label_conflict"};
    data.sets = {{}, {label_id("protein_coding")}};
    for (std::uint32_t id = 0; id != 8; ++id) {
        GeneRecord gene;
        gene.id_string = id;
        gene.type_set = 1;
        gene.equivalence[0] = gene.equivalence[1] = id;
        gene.competition[0] = gene.competition[1] = 0;
        data.genes.push_back(gene);
    }
    data.policies = {{0, 0, missing, 0}};
    const auto root = std::filesystem::temp_directory_path() / "pancollapse-numeric-assignment";
    std::filesystem::create_directories(root);
    const auto path = root / "reference.pcr";
    write(path, data);
    const auto reference = Reference::load(path);

    const auto pansc = effective_profile(ProfileId::pansc_strict_v1);
    const auto numeric_pansc = compile_numeric_profile(reference, pansc);
    require(std::find(numeric_pansc.assignment.excluded_categories.begin(),
                      numeric_pansc.assignment.excluded_categories.end(),
                      label_id("no_source_provenance")) !=
                numeric_pansc.assignment.excluded_categories.end(),
            "compiled profile did not retain referenced category");
    const auto absent = effective_profile(
        ProfileId::cr7_v1,
        {parse_profile_override("cr7-v1:excluded-categories=not_in_reference")});
    require(compile_numeric_profile(reference, absent).assignment.excluded_categories.empty(),
            "absent profile labels must be ignored during compilation");
    const auto projected = numeric_assignment_candidate(
        reference, numeric_pansc, 0, 17, EvidenceTier::exon, EvidenceStrand::forward, {});
    require(projected.gene == gene_id("A") && projected.equivalence_gene == gene_id("A"),
            "projection must retain numeric feature identities");

    // Equal support and gene identity forces the nested-host tie break. The
    // second alias has the numeric sentinel, matching the text empty string.
    auto host_alias = candidate("A", EvidenceTier::body, EvidenceStrand::forward);
    equivalence(host_alias, "C");
    nested_host(host_alias, "B");
    support(host_alias, {1});
    auto empty_alias = candidate("A", EvidenceTier::body, EvidenceStrand::forward);
    equivalence(empty_alias, "C");
    support(empty_alias, {2});
    auto peer = candidate("B", EvidenceTier::body, EvidenceStrand::forward);
    support(peer, {3});
    auto result = compare_case(reference, pansc, numeric_pansc,
                               {host_alias, empty_alias, peer}, {}, false,
                               "missing nested host differs from empty-string ordering");
    require(result.genes == std::vector<std::string>({"B", "C"}) &&
                !(result.reasons & reason_nested_host_preferred),
            "sentinel nested-host tie changed policy output");

    auto alias_a = candidate("A", EvidenceTier::body, EvidenceStrand::forward);
    equivalence(alias_a, "C");
    support(alias_a, {1, 2});
    auto alias_b = candidate("B", EvidenceTier::body, EvidenceStrand::forward);
    equivalence(alias_b, "C");
    support(alias_b, {2, 3, 4});
    auto runner = candidate("D", EvidenceTier::body, EvidenceStrand::forward);
    support(runner, {5});
    result = compare_case(reference, pansc, numeric_pansc, {alias_a, alias_b, runner}, {}, false,
                          "body-support union differs by representation");
    require(result.genes == std::vector<std::string>{"C"} &&
                (result.reasons & reason_body_support_preferred),
            "body support union did not dominate after equivalence collapse");

    auto strong = candidate("A", EvidenceTier::exon, EvidenceStrand::forward);
    category(strong, "no_source_provenance");
    strong.text.strong_local_support = strong.numeric.strong_local_support = true;
    result = compare_case(reference, pansc, numeric_pansc, {strong}, {}, false,
                          "strong category exemption differs by representation");
    require(result.genes == std::vector<std::string>{"A"} &&
                (result.reasons & reason_strong_support_exemption),
            "strong exemption was lost");
    auto protected_strong = strong;
    protected_strong.text.protected_primary_protein_nested_host =
        protected_strong.numeric.protected_primary_protein_nested_host = true;
    auto clean = candidate("B", EvidenceTier::exon, EvidenceStrand::forward);
    result = compare_case(reference, pansc, numeric_pansc, {protected_strong, clean}, {}, false,
                          "protected host category branch differs by representation");
    require(result.genes == std::vector<std::string>{"B"} &&
                !(result.reasons & reason_strong_support_exemption),
            "protected host incorrectly received strong exemption");
    const auto unprotected = effective_profile(
        ProfileId::pansc_strict_v1,
        {parse_profile_override("pansc-strict-v1:protect-primary-protein-nested-hosts=false")});
    result = compare_case(reference, unprotected, compile_numeric_profile(reference, unprotected),
                          {protected_strong, clean}, {}, false,
                          "unprotected host branch differs by representation");
    require(result.genes == std::vector<std::string>({"A", "B"}) &&
                (result.reasons & reason_strong_support_exemption),
            "unprotected host did not restore the exempt explanation");

    auto novel = candidate("A", EvidenceTier::exon, EvidenceStrand::forward);
    equivalence(novel, "C");
    novel.text.novel_paralog = novel.numeric.novel_paralog = true;
    novel.text.novel_origin_gene = "B";
    novel.numeric.novel_origin_gene = gene_id("B");
    for (const auto mode : {NovelParalogPolicy::lump, NovelParalogPolicy::separate,
                            NovelParalogPolicy::ignore}) {
        const char* value = mode == NovelParalogPolicy::lump ? "lump"
                          : mode == NovelParalogPolicy::separate ? "separate" : "ignore";
        const auto profile = effective_profile(
            ProfileId::pansc_strict_v1,
            {parse_profile_override(std::string("pansc-strict-v1:novel-paralog-policy=") + value)});
        result = compare_case(reference, profile, compile_numeric_profile(reference, profile),
                              {novel}, {}, false, "novel paralog mode differs by representation");
        const std::vector<std::string> wanted = mode == NovelParalogPolicy::lump ?
            std::vector<std::string>{"B"} : mode == NovelParalogPolicy::separate ?
            std::vector<std::string>{"A"} : std::vector<std::string>{};
        require(result.genes == wanted, "novel paralog output identity changed");
    }

    const auto both = effective_profile(
        ProfileId::pansc_strict_v1,
        {parse_profile_override("pansc-strict-v1:strand=both"),
         parse_profile_override("pansc-strict-v1:exact-strand-gene-fallback=false")});
    auto forward = candidate("A", EvidenceTier::body, EvidenceStrand::forward);
    support(forward, {1, 2, 3, 4});
    auto reverse = candidate("B", EvidenceTier::body, EvidenceStrand::reverse);
    support(reverse, {5});
    result = compare_case(reference, both, compile_numeric_profile(reference, both),
                          {reverse, forward}, {}, false,
                          "both-strand resolution differs by representation");
    require(result.genes == std::vector<std::string>{"A"} &&
                result.winning_strand == EvidenceStrand::forward,
            "both-strand deterministic body selection changed");
    const auto reverse_profile = effective_profile(
        ProfileId::pansc_strict_v1,
        {parse_profile_override("pansc-strict-v1:strand=reverse")});
    result = compare_case(reference, reverse_profile,
                          compile_numeric_profile(reference, reverse_profile),
                          {candidate("A", EvidenceTier::partial_exon, EvidenceStrand::forward),
                           candidate("B", EvidenceTier::exon, EvidenceStrand::reverse)}, {}, false,
                          "reverse-strand rank differs by representation");
    require(result.genes == std::vector<std::string>{"B"}, "reverse rank changed");

    auto pool_best = candidate("A", EvidenceTier::exon, EvidenceStrand::forward, 100);
    auto pool_late = candidate("B", EvidenceTier::exon, EvidenceStrand::forward, 94);
    auto other_pool = candidate("C", EvidenceTier::partial_exon, EvidenceStrand::forward, 1);
    result = compare_case(reference, pansc, numeric_pansc,
                          {pool_best, pool_late, other_pool}, {}, false,
                          "six-pool score window differs by representation");
    require(result.genes == std::vector<std::string>{"A"} &&
                (result.reasons & reason_score_window_pruned),
            "pool-local score pruning changed");
    result = compare_case(reference, pansc, numeric_pansc, {pool_best, other_pool}, {}, true,
                          "native prepruned bypass differs by representation");
    require(!(result.reasons & reason_score_window_pruned),
            "already-pruned native facts ran a second score window");

    auto anti = candidate("A", EvidenceTier::partial_exon, EvidenceStrand::reverse);
    auto retry = candidate("B", EvidenceTier::partial_exon, EvidenceStrand::forward);
    category(retry, "no_source_provenance");
    result = compare_case(reference, pansc, numeric_pansc, {anti, retry}, {}, false,
                          "post-resolution category retry differs by representation");
    require(result.genes == std::vector<std::string>{"B"} &&
                (result.reasons & reason_no_source_retry),
            "category retry did not precede opposite-strand terminal");
    auto fallback = candidate("B", EvidenceTier::gene, EvidenceStrand::forward);
    category(fallback, "no_source_provenance");
    result = compare_case(reference, pansc, numeric_pansc,
                          {candidate("A", EvidenceTier::exon, EvidenceStrand::reverse)},
                          {fallback}, false, "exact Gene fallback differs by representation");
    require(result.genes == std::vector<std::string>{"B"} &&
                (result.reasons & reason_gene_fallback),
            "exact Gene fallback changed");

    auto primary = candidate("A", EvidenceTier::exon, EvidenceStrand::forward);
    auto fallback_member = candidate("B", EvidenceTier::exon, EvidenceStrand::forward);
    fallback_member.text.competition = fallback_member.numeric.competition = CompetitionClass::fallback;
    result = compare_case(reference, pansc, numeric_pansc, {primary, fallback_member}, {}, false,
                          "membership suppression differs by representation");
    require(result.genes == std::vector<std::string>{"A"} &&
                (result.reasons & reason_fallback_suppressed),
            "fallback membership was retained beside primary");

    auto absent_identity = candidate("", EvidenceTier::exon, EvidenceStrand::forward);
    absent_identity.text.identity_known = absent_identity.numeric.identity_known = false;
    result = compare_case(reference, pansc, numeric_pansc, {absent_identity}, {}, false,
                          "missing identity differs by representation");
    require(result.terminal == AssignmentTerminal::unassigned_no_identity &&
                result.reaches_umi_filter,
            "missing numeric identity did not preserve UMI reachability");

    // An absent label cannot relax a candidate category. The native compiler
    // omits that label and its no-op retry; count eligibility must stay exact.
    auto absent_label_profile = pansc;
    absent_label_profile.profile.assignment.post_resolution_fallback_categories = {"absent-label"};
    const auto absent_label_numeric = compile_numeric_profile(reference, absent_label_profile);
    const auto reverse_exon = candidate("A", EvidenceTier::exon, EvidenceStrand::reverse);
    const auto sense_gene = candidate("B", EvidenceTier::gene, EvidenceStrand::forward);
    AssignmentFacts absent_text_facts;
    absent_text_facts.exact = {reverse_exon.text};
    absent_text_facts.gene_fallback = {sense_gene.text};
    NumericAssignmentFacts absent_numeric_facts;
    absent_numeric_facts.exact = {reverse_exon.numeric};
    absent_numeric_facts.gene_fallback = {sense_gene.numeric};
    const auto absent_expected = resolve_assignment(
        absent_label_profile, absent_text_facts, AssignmentAlgorithm::node_score_v1);
    const auto absent_observed = render_result(reference, resolve_numeric_assignment(
        absent_label_numeric, absent_numeric_facts));
    require(absent_expected.genes == std::vector<std::string>{"B"} &&
                absent_observed.genes == absent_expected.genes &&
                absent_observed.terminal == absent_expected.terminal &&
                absent_observed.winning_tier == absent_expected.winning_tier &&
                (absent_observed.reasons ^ absent_expected.reasons) == reason_no_source_retry,
            "removing an absent-label retry changed more than its diagnostic reason");

    std::filesystem::remove(path);
    std::filesystem::remove(root);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
