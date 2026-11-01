#include "count_reference.hpp"
#include "node_score_test_support.hpp"

#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace pancollapse;
using namespace pancollapse::count_reference;
using TextNodeScoreResolver = pancollapse::test::TextNodeScoreResolver;
using direct_count::EvidenceStrand;
using direct_count::EvidenceTier;

constexpr std::uint32_t exon_forward = 1, exon_reverse = 2;
constexpr std::uint32_t body_forward = 4;
constexpr std::uint32_t junction_along = 1, junction_reverse_complement = 2;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

Data base_data() {
    Data data;
    data.content_id = "node-score-resolver-fixture";
    data.strings = {"A", "B", "TXA", "TXB"};
    data.sets = {{}};
    for (std::uint32_t i = 0; i != 2; ++i) {
        GeneRecord gene;
        gene.id_string = i;
        gene.type_set = 0;
        gene.equivalence[0] = i;
        gene.competition[0] = 0;
        gene.equivalence[1] = i;
        gene.competition[1] = 0;
        data.genes.push_back(gene);
        data.policies.push_back({i, 0, missing, 0});
    }
    data.models = {{2, 0, 0, 0}, {3, 1, 1, 1}};
    return data;
}

void add_node(Data& data, std::uint64_t id, std::uint64_t length,
              std::vector<Posting> postings) {
    const auto first = static_cast<std::uint64_t>(data.postings.size());
    data.postings.insert(data.postings.end(), postings.begin(), postings.end());
    data.nodes.push_back({id, length, first, static_cast<std::uint32_t>(postings.size()), 0});
}

vg::Mapping* add_mapping(vg::Subpath* subpath, std::uint64_t id, bool reverse,
                         std::int64_t from, std::int64_t to, bool mismatch = false) {
    auto* mapping = subpath->mutable_path()->add_mapping();
    mapping->mutable_position()->set_node_id(id);
    mapping->mutable_position()->set_is_reverse(reverse);
    auto* edit = mapping->add_edit();
    edit->set_from_length(from);
    edit->set_to_length(to);
    if (mismatch) edit->set_sequence(std::string(static_cast<std::size_t>(to), 'C'));
    return mapping;
}

vg::MultipathAlignment linear(const std::vector<std::tuple<std::uint64_t, bool, int, int, bool>>& nodes) {
    vg::MultipathAlignment result;
    std::size_t read_bases = 0;
    for (const auto& node : nodes) read_bases += static_cast<std::size_t>(std::get<3>(node));
    result.set_sequence(std::string(read_bases, 'A'));
    auto* subpath = result.add_subpath();
    for (const auto& [id, reverse, from, to, mismatch] : nodes) {
        add_mapping(subpath, id, reverse, from, to, mismatch);
    }
    return result;
}

std::vector<direct_count::EffectiveProfile> profiles() {
    auto forward = direct_count::effective_profile(direct_count::ProfileId::cr7_v1);
    auto reverse = forward;
    reverse.profile.assignment.strand = direct_count::StrandPolicy::reverse;
    return {forward, reverse};
}

std::vector<ResolvedAssignment> resolve(const Reference& reference,
                                        const std::vector<const vg::MultipathAlignment*>& records,
                                        std::span<const direct_count::EffectiveProfile> profile_set) {
    return TextNodeScoreResolver(reference).resolve(records, profile_set);
}

const direct_count::AssignmentCandidate& only_exact(const ResolvedAssignment& assignment,
                                                     const char* message) {
    require(assignment.facts.exact.size() == 1, message);
    return assignment.facts.exact.front();
}

const direct_count::AssignmentCandidate& exact_for(const ResolvedAssignment& assignment,
                                                    const std::string& gene) {
    for (const auto& candidate : assignment.facts.exact) {
        if (candidate.gene == gene) return candidate;
    }
    throw std::runtime_error("missing exact candidate");
}

Reference write_load(const std::filesystem::path& path, const Data& data) {
    write(path, data);
    return Reference::load(path);
}

}  // namespace

int main() try {
    const auto root = std::filesystem::temp_directory_path() / "pancollapse-node-score-resolver-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto profile_set = profiles();

    // D091: alternative COMPLETE routes and separate records retain their best
    // score, rather than accumulating mutually exclusive route/record evidence.
    Data routes = base_data();
    add_node(routes, 1, 10, {{0, 0, exon_forward}});
    auto route_reference = write_load(root / "routes.pcr", routes);
    vg::MultipathAlignment branches;
    branches.set_sequence(std::string(10, 'A'));
    auto* branch_a = branches.add_subpath();
    auto* branch_b = branches.add_subpath();
    add_mapping(branch_a, 1, false, 10, 10);
    add_mapping(branch_b, 1, false, 10, 10);
    const auto branch_result = resolve(route_reference, {&branches}, profile_set);
    require(only_exact(branch_result[0], "branch fixture must retain one condensed E winner").score == 20,
            "mutually exclusive COMPLETE routes must not sum scores");
    const auto record_a = linear({{1, false, 10, 10, false}});
    const auto record_b = linear({{1, false, 10, 10, false}});
    const auto record_result = resolve(route_reference, {&record_a, &record_b}, profile_set);
    require(only_exact(record_result[0], "records must retain one condensed E winner").score == 20,
            "separate records must not sum scores");

    // Alternatives may converge; each route is still scored independently.
    vg::MultipathAlignment converge;
    converge.set_sequence(std::string(20, 'A'));
    auto* left = converge.add_subpath(); left->add_next(2);
    auto* right = converge.add_subpath(); right->add_next(2);
    auto* shared = converge.add_subpath();
    add_mapping(left, 1, false, 10, 10);
    add_mapping(right, 1, false, 10, 10);
    add_mapping(shared, 1, false, 10, 10);
    const auto converge_result = resolve(route_reference, {&converge}, profile_set);
    require(only_exact(converge_result[0], "convergent alternatives must retain an E winner").score == 30,
            "convergent alternatives must not double-count the shared suffix");

    // Coverage uses consumed reference bases, not the full graph-node length.
    Data coverage = base_data();
    add_node(coverage, 2, 100, {{0, 0, exon_forward}});
    add_node(coverage, 3, 100, {{0, 0, body_forward}});
    add_node(coverage, 4, 100, {});
    auto coverage_reference = write_load(root / "coverage.pcr", coverage);
    const auto mapped_only = linear({{2, false, 40, 40, false}});
    require(only_exact(resolve(coverage_reference, {&mapped_only}, profile_set)[0],
                       "mapped segment must produce exact evidence").tier == EvidenceTier::exon,
            "a fully exonic mapped segment is E regardless of node length");
    const auto half = linear({{2, false, 10, 10, false}, {3, false, 10, 10, false}});
    require(only_exact(resolve(coverage_reference, {&half}, profile_set)[0],
                       "half-exonic read must produce exact evidence").tier == EvidenceTier::partial_exon,
            "50 percent exon coverage is P");
    const auto body = linear({{2, false, 9, 9, false}, {3, false, 11, 11, false}});
    require(only_exact(resolve(coverage_reference, {&body}, profile_set)[0],
                       "body read must produce exact evidence").tier == EvidenceTier::body,
            "below 50 percent exon coverage with body evidence is B");
    const auto deletion = linear({{2, false, 5, 5, false}, {4, false, 5, 0, false}});
    require(only_exact(resolve(coverage_reference, {&deletion}, profile_set)[0],
                       "deletion fixture must retain exon evidence").tier == EvidenceTier::partial_exon,
            "reference-consuming deletion bases remain in the coverage denominator");

    // Posting orientation is relative to the aligned path, and aligned bases
    // (not node visits) determine the majority strand.
    Data orientation = base_data();
    add_node(orientation, 4, 100, {{0, 0, exon_forward}, {1, 0, exon_reverse}});
    auto orientation_reference = write_load(root / "orientation.pcr", orientation);
    const auto majority = linear({{4, false, 30, 30, false}, {4, true, 10, 10, false}});
    const auto orientation_result = resolve(orientation_reference, {&majority}, profile_set);
    require(exact_for(orientation_result[0], "A").strand == EvidenceStrand::forward &&
                exact_for(orientation_result[0], "B").strand == EvidenceStrand::reverse,
            "sense/antisense voting uses majority aligned bases");
    require(exact_for(orientation_result[1], "A").strand == EvidenceStrand::forward &&
                exact_for(orientation_result[1], "B").strand == EvidenceStrand::reverse,
            "reverse posting is interpreted relative to the path orientation");
    require(orientation_result[0].result.genes == std::vector<std::string>{"A"} &&
                orientation_result[1].result.genes == std::vector<std::string>{"B"},
            "profile selection, not fact collection, resolves opposite orientations");

    // The E veto is evaluated before a lower-tier P candidate on the other strand.
    Data veto = base_data();
    add_node(veto, 5, 20, {{0, 0, body_forward}, {1, 0, exon_reverse}});
    add_node(veto, 6, 20, {{0, 0, exon_forward}, {1, 0, exon_reverse}});
    auto veto_reference = write_load(root / "veto.pcr", veto);
    const auto veto_read = linear({{5, false, 10, 10, false}, {6, false, 10, 10, false}});
    const auto veto_result = resolve(veto_reference, {&veto_read}, profile_set);
    require(veto_result[0].facts.exact.size() == 2 &&
                veto_result[0].result.terminal == direct_count::AssignmentTerminal::unassigned_opposite_strand &&
                veto_result[1].result.genes == std::vector<std::string>{"B"},
            "antisense E veto must take precedence over sense P");
    const std::vector<direct_count::EffectiveProfile> custom_profiles{
        direct_count::effective_profile(direct_count::ProfileId::pansc_strict_v1), profile_set[0]};
    const auto fallback_result = resolve(veto_reference, {&veto_read}, custom_profiles);
    require(fallback_result[0].result.genes == std::vector<std::string>{"A"} &&
                fallback_result[0].result.winning_tier == EvidenceTier::gene &&
                fallback_result[1].result.terminal == direct_count::AssignmentTerminal::unassigned_opposite_strand,
            "custom downstream Gene fallback remains explicit and distinct from the CR7 veto");

    // The inclusive five-point window is independent for every (tier, strand),
    // so a lower P pool remains available while E ties remain visible.
    Data window = base_data();
    add_node(window, 7, 10, {{0, 0, exon_forward}, {1, 0, exon_forward}});
    add_node(window, 8, 10, {{0, 0, exon_forward}});
    auto window_reference = write_load(root / "window.pcr", window);
    const auto within_five = linear({{7, false, 10, 10, false}, {8, false, 10, 10, true}});
    const auto window_result = resolve(window_reference, {&within_five}, profile_set);
    require(window_result[0].facts.exact.size() == 2,
            "a lower evidence pool must not be pruned by the E pool");
    direct_count::AssignmentFacts scored;
    scored.exact = orientation_result[0].facts.exact;
    for (auto& candidate : scored.exact) {
        candidate.strand = EvidenceStrand::forward;
        candidate.score = candidate.gene == "A" ? 20 : 15;
    }
    require(direct_count::resolve_assignment(profile_set[0], scored,
                direct_count::AssignmentAlgorithm::node_score_v1).genes == std::vector<std::string>({"A", "B"}),
            "exactly five score points below a pool maximum is retained");
    for (auto& candidate : scored.exact) if (candidate.gene == "B") candidate.score = 14;
    require(direct_count::resolve_assignment(profile_set[0], scored,
                direct_count::AssignmentAlgorithm::node_score_v1).genes == std::vector<std::string>{"A"},
            "six score points below a pool maximum is removed");

    // Same-transcript aliases retain policy payloads only at their exact pooled
    // maximum. A lower alias within five points is not an independent candidate.
    Data aliases = base_data();
    aliases.models = {{2, 0, 0, 0}, {2, 1, 1, 0}};
    aliases.policies[1] = {1, 0, missing, 0};
    add_node(aliases, 20, 10, {{0, 0, exon_forward}});
    add_node(aliases, 21, 5, {{1, 0, exon_forward}});
    auto alias_reference = write_load(root / "aliases.pcr", aliases);
    const auto high = linear({{20, false, 10, 10, false}});
    const auto low = linear({{21, false, 5, 5, false}});
    const auto alias_result = resolve(alias_reference, {&high, &low}, profile_set);
    require(alias_result[0].facts.exact.size() == 1 &&
                alias_result[0].facts.exact.front().gene == "A" &&
                alias_result[0].facts.exact.front().score == 20,
            "a same-transcript lower alias within five points must not survive the pooled maximum");

    // Known splice direction rejects only its target candidate. Ordinary edges
    // do not become containment or ordering requirements for another target.
    Data splice = base_data();
    add_node(splice, 9, 10, {{0, 0, exon_forward}, {1, 0, exon_forward}});
    add_node(splice, 10, 10, {{0, 0, exon_forward}, {1, 0, exon_forward}});
    splice.splice_records = {{9, 10, 0, 2, 0}};
    splice.splice_targets = {2, 3};
    splice.junction_records = {{18, 20, 0, 2, 0}};
    splice.junction_owners = {{2, junction_reverse_complement}, {3, junction_along | junction_reverse_complement}};
    auto splice_reference = write_load(root / "splice.pcr", splice);
    // Partial endpoint mappings and an insertion-only bridge still observe the
    // indexed physical splice; they do not require endpoint containment.
    vg::MultipathAlignment splice_read;
    splice_read.set_sequence(std::string(12, 'A'));
    auto* splice_path = splice_read.add_subpath();
    add_mapping(splice_path, 9, false, 5, 5);
    add_mapping(splice_path, 10, false, 0, 2);
    add_mapping(splice_path, 10, false, 5, 5);
    const auto splice_result = resolve(splice_reference, {&splice_read}, profile_set);
    require(splice_result[0].facts.exact.size() == 1 && splice_result[0].facts.exact.front().gene == "B",
            "wrong-direction known splice rejects only its target candidate");
    splice.junction_owners[1].flags = junction_reverse_complement;
    auto rejected_reference = write_load(root / "rejected.pcr", splice);
    const auto rejected = resolve(rejected_reference, {&splice_read}, profile_set);
    require(rejected[0].facts.exact.empty() && rejected[0].result.genes.empty() &&
                !rejected[0].result.barcode_correction_eligible && !rejected[0].result.reaches_umi_filter,
            "all splice-vetoed candidates have no gene call or diagnostic shadow assignment");

    std::filesystem::remove_all(root);
    std::cout << "node score resolver checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
