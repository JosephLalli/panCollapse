#include "count_reference.hpp"
#include "node_score_test_support.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace pancollapse;
using namespace pancollapse::count_reference;
using TextNodeScoreResolver = pancollapse::test::TextNodeScoreResolver;
using direct_count::AssignmentCandidate;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

Data data() {
    Data value;
    value.content_id = "node-score-dp-differential";
    value.strings = {"A", "B", "C", "D", "TXA", "TXB", "TXC", "TXD"};
    value.sets = {{}};
    for (std::uint32_t i = 0; i != 4; ++i) {
        GeneRecord gene;
        gene.id_string = i;
        gene.type_set = 0;
        gene.equivalence[0] = i;
        gene.competition[0] = 0;
        gene.equivalence[1] = i;
        gene.competition[1] = 0;
        value.genes.push_back(gene);
        value.policies.push_back({i, 0, missing, 0});
        value.models.push_back({4 + i, i, i, i & 1u});
    }
    return value;
}

void node(Data& value, std::uint64_t id, std::uint64_t length,
          std::vector<Posting> postings) {
    const auto first = static_cast<std::uint64_t>(value.postings.size());
    value.postings.insert(value.postings.end(), postings.begin(), postings.end());
    value.nodes.push_back({id, length, first, static_cast<std::uint32_t>(postings.size()), 0});
}

void map(vg::Subpath* path, std::uint64_t id, bool reverse, int from, int to, bool mismatch = false) {
    auto* mapping = path->mutable_path()->add_mapping();
    mapping->mutable_position()->set_node_id(id);
    mapping->mutable_position()->set_is_reverse(reverse);
    auto* edit = mapping->add_edit();
    edit->set_from_length(from);
    edit->set_to_length(to);
    if (mismatch) edit->set_sequence(std::string(static_cast<std::size_t>(to), 'C'));
}

void map_with_insertion(vg::Subpath* path, std::uint64_t id) {
    auto* mapping = path->mutable_path()->add_mapping();
    mapping->mutable_position()->set_node_id(id);
    auto* match = mapping->add_edit(); match->set_from_length(1); match->set_to_length(1);
    auto* insertion = mapping->add_edit(); insertion->set_from_length(0); insertion->set_to_length(1);
}

vg::MultipathAlignment dag(const std::vector<std::vector<std::tuple<std::uint64_t, bool, int, int>>>& paths,
                           const std::vector<std::vector<int>>& next) {
    require(next.size() == paths.size(), "DAG paths/next cardinality mismatch");
    vg::MultipathAlignment result;
    // GAMP alternatives share one read. Set its length to the longest complete
    // read-consuming route, rather than summing mutually exclusive subpaths.
    std::vector<std::size_t> suffix(paths.size());
    std::size_t bases = 0;
    for (std::size_t reverse_i = paths.size(); reverse_i != 0; --reverse_i) {
        const std::size_t i = reverse_i - 1;
        std::size_t own = 0, child = 0;
        for (const auto& step : paths[i]) own += static_cast<std::size_t>(std::get<3>(step));
        for (const int successor : next[i]) {
            require(successor > static_cast<int>(i) && successor < static_cast<int>(paths.size()),
                    "DAG successor must be a later subpath");
            child = std::max(child, suffix[successor]);
        }
        bases = std::max(bases, suffix[i] = own + child);
    }
    result.set_sequence(std::string(bases ? bases : 1, 'A'));
    for (std::size_t i = 0; i != paths.size(); ++i) {
        auto* subpath = result.add_subpath();
        for (const auto& [id, reverse, from, to] : paths[i]) map(subpath, id, reverse, from, to);
        for (const int successor : next[i]) subpath->add_next(successor);
    }
    return result;
}

std::vector<direct_count::EffectiveProfile> profiles() {
    auto cr7 = direct_count::effective_profile(direct_count::ProfileId::cr7_v1);
    auto strict = direct_count::effective_profile(direct_count::ProfileId::pansc_strict_v1);
    return {cr7, strict};
}

void canonical(std::vector<ResolvedAssignment>& assignments) {
    const auto candidate_less = [](const AssignmentCandidate& a, const AssignmentCandidate& b) {
        return std::tie(a.gene, a.equivalence_gene, a.nested_host, a.score, a.tier, a.strand,
                        a.competition, a.categories, a.gene_types, a.body_support_units,
                        a.body_sample_support, a.identity_known, a.novel_paralog, a.novel_origin_gene,
                        a.strong_local_support, a.protected_primary_protein_nested_host) <
               std::tie(b.gene, b.equivalence_gene, b.nested_host, b.score, b.tier, b.strand,
                        b.competition, b.categories, b.gene_types, b.body_support_units,
                        b.body_sample_support, b.identity_known, b.novel_paralog, b.novel_origin_gene,
                        b.strong_local_support, b.protected_primary_protein_nested_host);
    };
    for (auto& assignment : assignments) {
        std::sort(assignment.facts.exact.begin(), assignment.facts.exact.end(), candidate_less);
        std::sort(assignment.facts.gene_fallback.begin(), assignment.facts.gene_fallback.end(), candidate_less);
        std::sort(assignment.result.genes.begin(), assignment.result.genes.end());
    }
}

bool same(std::vector<ResolvedAssignment> left, std::vector<ResolvedAssignment> right) {
    canonical(left); canonical(right);
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i != left.size(); ++i) {
        const auto& a = left[i]; const auto& b = right[i];
        if (a.facts.has_complete_provenance != b.facts.has_complete_provenance ||
            a.facts.exact != b.facts.exact || a.facts.gene_fallback != b.facts.gene_fallback ||
            a.result.genes != b.result.genes) {
            std::cerr << "differential profile=" << i << " facts exact="
                      << a.facts.exact.size() << '/' << b.facts.exact.size()
                      << " fallback=" << a.facts.gene_fallback.size() << '/'
                      << b.facts.gene_fallback.size() << '\n';
            for (const auto* side : {&a, &b}) {
                std::cerr << "genes:";
                for (const auto& gene : side->result.genes) std::cerr << ' ' << gene;
                for (const auto& row : side->facts.exact)
                    std::cerr << " [" << row.gene << ':' << row.score << ':'
                              << static_cast<int>(row.tier) << ':' << static_cast<int>(row.strand) << ']';
                std::cerr << '\n';
            }
            return false;
        }
        // The frozen oracle retains splice-rejected shadow diagnostics. These
        // cannot contribute a molecule and are deliberately absent from count.
        if (a.result.genes.empty()) continue;
        // Compiling away a fallback label absent from this reference also
        // removes its guaranteed-no-op retry and its diagnostic reason bit.
        if (a.result.terminal != b.result.terminal ||
            ((a.result.reasons ^ b.result.reasons) & ~direct_count::reason_no_source_retry) != 0 ||
            a.result.winning_tier != b.result.winning_tier ||
            a.result.winning_strand != b.result.winning_strand ||
            a.result.barcode_correction_eligible != b.result.barcode_correction_eligible ||
            a.result.reaches_umi_filter != b.result.reaches_umi_filter) {
            std::cerr << "differential diagnostics profile=" << i
                      << " reasons=" << a.result.reasons << '/' << b.result.reasons
                      << " terminal=" << static_cast<int>(a.result.terminal) << '/'
                      << static_cast<int>(b.result.terminal) << '\n';
            return false;
        }
    }
    return true;
}

const AssignmentCandidate& candidate_for(const ResolvedAssignment& assignment, const std::string& gene) {
    for (const auto& candidate : assignment.facts.exact) if (candidate.gene == gene) return candidate;
    throw std::runtime_error("missing expected exact candidate");
}

const AssignmentCandidate& candidate_for(const ResolvedAssignment& assignment,
                                         direct_count::EvidenceTier tier) {
    for (const auto& candidate : assignment.facts.exact) if (candidate.tier == tier) return candidate;
    throw std::runtime_error("missing expected exact tier candidate");
}

Reference write_load(const std::filesystem::path& path, const Data& value) {
    write(path, value);
    return Reference::load(path);
}

class MembershipViewSource : public Source {
  public:
    MembershipViewSource(const Source& source, bool share,
                         std::vector<std::pair<std::uint64_t, std::uint32_t>> ids = {{100, 7}, {101, 7}})
        : source_(source), share_(share), ids_(std::move(ids)) {}
    const std::vector<std::string>& strings() const override { return source_.strings(); }
    const std::vector<std::vector<std::uint32_t>>& sets() const override { return source_.sets(); }
    std::span<const GeneRecord> genes() const override { return source_.genes(); }
    std::span<const PolicyRecord> policies() const override { return source_.policies(); }
    const std::string& content_id() const override { return source_.content_id(); }
    std::size_t model_count() const override { return source_.model_count(); }
    ModelRecord model(std::uint32_t id) const override { return source_.model(id); }
    std::size_t scoring_class_count() const override { return source_.scoring_class_count(); }
    ScoringClassRecord scoring_class(std::uint32_t id) const override { return source_.scoring_class(id); }
    const NodeRecord* node(std::uint64_t id) const override { return source_.node(id); }
    std::span<const Posting> node_postings(const NodeRecord& record) const override {
        return source_.node_postings(record);
    }
    std::span<const ScoringPosting> node_scoring(const NodeRecord& record) const override {
        return source_.node_scoring(record);
    }
    std::span<const ScoringBinding> scoring_bindings(const ScoringClassRecord& record) const override {
        return source_.scoring_bindings(record);
    }
    std::span<const EdgePosting> edge(std::uint64_t from, std::uint64_t to) const override {
        return source_.edge(from, to);
    }
    std::span<const std::uint32_t> splice_targets(std::uint64_t a, std::uint64_t b) const override {
        return source_.splice_targets(a, b);
    }
    std::span<const JunctionOwner> junction_owners(std::uint64_t from, std::uint64_t to) const override {
        return source_.junction_owners(from, to);
    }
    std::optional<NodeView> node_view(std::uint64_t id) const override {
        auto view = source_.node_view(id);
        if (view) {
            view->shared_membership_id = missing;
            if (share_) for (const auto& [node, list] : ids_)
                if (node == id) { view->shared_membership_id = list; break; }
        }
        return view;
    }
    void begin_read_group() const override { source_.begin_read_group(); }

  private:
    const Source& source_;
    bool share_ = false;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> ids_;
};

class ReadyModelSource final : public MembershipViewSource {
  public:
    ReadyModelSource(const Reference& source, std::size_t limit)
        : MembershipViewSource(source, true), models_(source.models().first(limit)) {}
    std::span<const ModelRecord> ready_models() const override { return models_; }
    ModelRecord model(std::uint32_t) const override {
        throw std::runtime_error("ready source must not perform per-model lookups");
    }
  private:
    std::span<const ModelRecord> models_;
};

struct DifferentialModes {
    std::vector<ResolvedAssignment> oracle, sparse, automatic;
    NodeScoreStats sparse_stats, automatic_stats;
};

DifferentialModes differential_modes(const Source& reference,
                                     const std::vector<const vg::MultipathAlignment*>& records,
                                     const char* name) {
    const auto profile_set = profiles();
    TextNodeScoreResolver oracle_resolver(reference, NodeScoreExecution::enumerate_oracle);
    TextNodeScoreResolver sparse_resolver(reference, NodeScoreExecution::force_sparse);
    TextNodeScoreResolver automatic_resolver(reference, NodeScoreExecution::automatic);
    TextNodeScoreResolver map_resolver(reference, NodeScoreExecution::force_map);
    auto oracle = oracle_resolver.resolve(records, profile_set);
    auto sparse = sparse_resolver.resolve(records, profile_set);
    auto automatic = automatic_resolver.resolve(records, profile_set);
    require(same(oracle, sparse), name);
    require(same(oracle, automatic), name);
    require(same(oracle, map_resolver.resolve(records, profile_set)), name);
    for (const std::size_t batch : {1U, 2U, 7U})
        require(same(oracle, TextNodeScoreResolver(reference, NodeScoreExecution::automatic, batch)
                                .resolve(records, profile_set)), name);
    return {std::move(oracle), std::move(sparse), std::move(automatic), sparse_resolver.stats(),
            automatic_resolver.stats()};
}

void differential(const Reference& reference, const std::vector<const vg::MultipathAlignment*>& records,
                  const char* name) {
    (void)differential_modes(reference, records, name);
}

void threshold_bypass_cases(const std::filesystem::path& root) {
    auto facts = data();
    for (std::uint64_t id = 800; id < 804; ++id) node(facts, id, 1, {{0, 0, 5}});
    auto reference = write_load(root / "no-join.pcr", facts);
    const auto check = [&](const vg::MultipathAlignment& read, bool bypass) {
        const auto modes = differential_modes(reference, {&read},
            "no-join bypass must preserve oracle, sparse, map and frontier-batch parity");
        require((modes.automatic_stats.balance_prepare_bypassed > 0) == bypass,
                "only one-start DAGs without joins may bypass threshold preparation");
        require(modes.sparse_stats.balance_prepare_bypassed == 0,
                "explicit sparse test control must not use automatic bypass");
    };
    check(dag({{{800, false, 1, 1}}, {{801, false, 1, 1}}, {{802, false, 1, 1}}},
              {{1}, {2}, {}}), true);
    check(dag({{{800, false, 1, 1}}, {{801, false, 1, 1}}, {{802, false, 1, 1}}},
              {{1, 2}, {}, {}}), true);
    check(dag({{{800, false, 1, 1}}, {{801, false, 1, 1}}, {{802, false, 1, 1}},
               {{803, false, 1, 1}}}, {{1, 2}, {3}, {3}, {}}), false);
    auto multiple_starts = dag({{{800, false, 1, 1}}, {{801, false, 1, 1}}}, {{}, {}});
    multiple_starts.add_start(0); multiple_starts.add_start(1);
    check(multiple_starts, false);
    auto duplicate_start = multiple_starts;
    duplicate_start.set_start(1, 0);
    check(duplicate_start, false);
    check(dag({{{800, false, 1, 1}}, {{801, false, 1, 1}}}, {{1, 1}, {}}), false);
    auto duplicate_connection = dag({{{800, false, 1, 1}}, {{801, false, 1, 1}}}, {{1}, {}});
    duplicate_connection.mutable_subpath(0)->add_connection()->set_next(1);
    check(duplicate_connection, false);
    auto unreachable_join = dag({{{800, false, 1, 1}}, {{801, false, 1, 1}},
                                 {{802, false, 1, 1}}}, {{}, {2, 2}, {}});
    unreachable_join.add_start(0);
    check(unreachable_join, false);
    for (unsigned invalid = 0; invalid < 2; ++invalid) {
        auto malformed = duplicate_connection;
        if (invalid == 0) malformed.add_start(100);
        else malformed.mutable_subpath(0)->mutable_connection(0)->set_next(0);
        bool rejected = false;
        try { (void)TextNodeScoreResolver(reference).resolve({&malformed}, profiles()); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "topology/start validation must precede no-join classification");
    }
}

}  // namespace

int main() try {
    const auto root = std::filesystem::temp_directory_path() / "pancollapse-node-score-dp-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    threshold_bypass_cases(root);

    // Named hostile fixture: threshold crossing, late strand flips, all four
    // splice masks, insertion bridge, deletion/reference coverage, repeated
    // nodes, multiple source/sink paths, and unannotated evidence.
    Data hostile = data();
    hostile.sets = {{}, {10}, {20}, {30}};
    node(hostile, 1, 10, {{0, 0, 1}, {1, 0, 1}});
    node(hostile, 2, 10, {{0, 1, 4}, {1, 2, 8}, {2, 0, 2}});
    node(hostile, 3, 10, {{1, 0, 1}, {3, 3, 4}});
    node(hostile, 4, 10, {}); // unannotated deletion endpoint
    node(hostile, 5, 10, {{2, 0, 1}, {3, 0, 2}});
    hostile.splice_records = {{1, 2, 0, 4, 0}};
    hostile.splice_targets = {4, 5, 6, 7};
    hostile.junction_records = {{2, 4, 0, 3, 0}};
    // A missing owner is the on-disk 00 mask; the codec forbids an explicit
    // zero-flags owner row. The three rows encode 01/10/11.
    hostile.junction_owners = {{5, 1}, {6, 2}, {7, 3}};
    auto hostile_reference = write_load(root / "hostile.pcr", hostile);
    auto adverse = dag({{{1, false, 10, 10}}, {}, {{2, false, 0, 1}}, {{2, false, 5, 5}},
                        {{3, true, 10, 10}}, {{4, false, 6, 0}},
                        {{5, false, 10, 10}, {2, false, 1, 1}}},
                       {{1, 2, 3}, {3}, {3}, {4}, {}, {}, {}});
    map_with_insertion(adverse.mutable_subpath(3), 2);
    adverse.set_sequence(std::string(28, 'A'));
    differential(hostile_reference, {&adverse},
                 "adverse fixture: threshold/masks/orientation/bridges/deletions/repeats");
    require(hostile_reference.node_view(1)->shared_membership_id == missing,
            "generic raw references must not claim a shared posting-list identity");

    // Two differently sized nodes intentionally share their exact ordered
    // postings. A third list overlaps one model, while the 100->101 edge is
    // directional, so shared membership must retain actual node credits/edges.
    Data shared_memberships = data();
    shared_memberships.sets = {{}, {10}, {20}, {30}};
    const std::vector<Posting> shared_rows{{0, 1, 5}, {1, 2, 10}};
    node(shared_memberships, 100, 5, shared_rows);
    node(shared_memberships, 101, 7, shared_rows);
    node(shared_memberships, 102, 5, {{1, 2, 4}, {2, 3, 4}});
    shared_memberships.splice_records = {{100, 101, 0, 2, 0}};
    shared_memberships.splice_targets = {4, 5};
    shared_memberships.junction_records = {{200, 203, 0, 2, 0}};
    shared_memberships.junction_owners = {{4, 1}, {5, 2}};
    auto shared_reference = write_load(root / "shared-memberships.pcr", shared_memberships);
    MembershipViewSource shared_source(shared_reference, true);
    MembershipViewSource missing_source(shared_reference, false);
    auto shared_read = dag({{{100, false, 5, 5}}, {{101, true, 7, 7}}, {{102, false, 5, 5}}},
                           {{1}, {2}, {}});
    const auto shared_modes = differential_modes(shared_source, {&shared_read},
                                                  "shared membership lists preserve resolver oracle parity");
    const auto missing_modes = differential_modes(missing_source, {&shared_read},
                                                   "missing membership IDs preserve resolver oracle parity");
    require(shared_source.ready_models().empty(), "generic source must retain model lookup fallback");
    ReadyModelSource ready_source(shared_reference, shared_reference.model_count());
    require(same(TextNodeScoreResolver(ready_source).resolve({&shared_read}, profiles()), shared_modes.automatic),
            "direct model table must preserve fallback-source assignments");
    ReadyModelSource short_source(shared_reference, 1);
    bool short_rejected = false;
    try { (void)TextNodeScoreResolver(short_source).resolve({&shared_read}, profiles()); }
    catch (const std::runtime_error&) { short_rejected = true; }
    require(short_rejected, "model IDs outside ready table must fail before indexing");
    require(same(shared_modes.automatic, missing_modes.automatic) &&
                shared_modes.automatic_stats.nodes_with_shared_id == 2 &&
                shared_modes.automatic_stats.membership_lists_decoded == 2 &&
                shared_modes.automatic_stats.membership_lists_reused == 1 &&
                shared_modes.automatic_stats.posting_rows_decoded == 4 &&
                shared_modes.automatic_stats.posting_rows_reused == 2 &&
                missing_modes.automatic_stats.nodes_with_shared_id == 0 &&
                missing_modes.automatic_stats.membership_lists_decoded == 3 &&
                missing_modes.automatic_stats.membership_lists_reused == 0 &&
                missing_modes.automatic_stats.posting_rows_decoded == 6 &&
                shared_modes.automatic_stats.incidence_node_rows == 6 &&
                shared_modes.automatic_stats.incidence_list_rows == 4 &&
                missing_modes.automatic_stats.incidence_node_rows == 6 &&
                missing_modes.automatic_stats.incidence_list_rows == 6 &&
                shared_modes.automatic_stats.membership_groups == missing_modes.automatic_stats.membership_groups &&
                shared_modes.automatic_stats.model_groups == missing_modes.automatic_stats.model_groups &&
                shared_modes.automatic_stats.model_visits == missing_modes.automatic_stats.model_visits,
            "shared membership must preserve class transfers while reducing incidence rows");

    // Equal payload under distinct IDs remains legal; unequal flags/supports
    // remain distinct. Record-local subsets, repeated/partial/reverse credits,
    // zero-base nodes, mixed support and a shared empty list exercise the
    // coordinate quotient independently of the node-by-node oracle.
    node(shared_memberships, 103, 9, shared_rows);
    node(shared_memberships, 104, 3, {{0, 3, 4}});
    node(shared_memberships, 105, 2, {});
    node(shared_memberships, 106, 4, {});
    auto subset_reference = write_load(root / "shared-membership-subsets.pcr", shared_memberships);
    MembershipViewSource subset_source(subset_reference, true,
        {{100, 7}, {101, 7}, {102, 8}, {103, 9}, {104, 10}, {105, 0}, {106, 0}});
    MembershipViewSource subset_missing(subset_reference, false);
    auto subset_one = dag({{{100, false, 3, 3}, {100, false, 2, 2}},
                           {{101, true, 4, 4}}, {{102, false, 5, 5}}}, {{1}, {2}, {}});
    auto subset_two = dag({{{101, false, 3, 3}, {103, true, 0, 0}},
                           {{104, false, 3, 3}}, {{105, false, 1, 1}, {106, true, 2, 2}}},
                          {{1}, {2}, {}});
    const auto subset_shared_modes = differential_modes(subset_source, {&subset_one, &subset_two},
        "list incidence must preserve partial/repeated/empty/mixed-support node facts");
    const auto subset_missing_modes = differential_modes(subset_missing, {&subset_one, &subset_two},
        "node incidence control must preserve partial/repeated/empty/mixed-support facts");
    const auto& subset_stats = subset_shared_modes.automatic_stats;
    const auto& subset_control = subset_missing_modes.automatic_stats;
    require(same(subset_shared_modes.automatic, subset_missing_modes.automatic) &&
                subset_stats.nodes_with_shared_id == 7 && subset_stats.membership_lists_decoded == 5 &&
                subset_stats.membership_lists_reused == 2 && subset_stats.posting_rows_decoded == 7 &&
                subset_stats.posting_rows_reused == 2 && subset_stats.incidence_node_rows == 11 &&
                subset_stats.incidence_list_rows == 9 && subset_control.incidence_node_rows == 11 &&
                subset_control.incidence_list_rows == 11 &&
                subset_stats.membership_groups == subset_control.membership_groups &&
                subset_stats.model_groups == subset_control.model_groups &&
                subset_stats.model_visits == subset_control.model_visits &&
                subset_stats.dp_states == subset_control.dp_states &&
                subset_stats.dp_transitions == subset_control.dp_transitions,
            "shared-list quotient must preserve record-local partitions and DP work");
    (void)differential_modes(subset_source, {&subset_one, &subset_two, &subset_one},
        "repeated alternatives must preserve independent alignment facts");

    TextNodeScoreResolver repeated_resolver(shared_source);
    const auto repeated_profiles = profiles();
    std::vector<std::vector<std::tuple<std::uint64_t, bool, int, int>>> large_paths(120, {{100, false, 3, 3}});
    std::vector<std::vector<int>> large_next(120);
    for (int i = 0; i < 119; ++i) large_next[i].push_back(i + 1);
    auto large_read = dag(large_paths, large_next);
    const auto large_expected = TextNodeScoreResolver(shared_source, NodeScoreExecution::enumerate_oracle)
                                    .resolve({&large_read}, repeated_profiles);
    require(same(repeated_resolver.resolve({&large_read}, repeated_profiles), large_expected),
            "large record must preserve explicit-route oracle");
    for (unsigned repeat = 0; repeat < 8; ++repeat)
        require(same(repeated_resolver.resolve({&shared_read}, repeated_profiles), shared_modes.automatic),
                "independent calls after an outlier must preserve assignments");
    vg::MultipathAlignment empty_record;
    (void)differential_modes(shared_source, {&empty_record}, "zero-subpath record parity");
    (void)differential_modes(shared_source, {}, "empty read group parity");
    auto unannotated_read = dag({{{105, false, 1, 1}}}, {{}});
    (void)differential_modes(subset_source, {&unannotated_read}, "annotation-free record parity");
    auto invalid_read = shared_read;
    invalid_read.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->mutable_position()->set_offset(9999);
    bool invalid_rejected = false;
    try { (void)repeated_resolver.resolve({&invalid_read}, repeated_profiles); }
    catch (const std::runtime_error&) { invalid_rejected = true; }
    require(invalid_rejected &&
                same(repeated_resolver.resolve({&shared_read}, repeated_profiles), shared_modes.automatic),
            "an exception must not alter later independent assignments");

    std::array<std::exception_ptr, 2> concurrent_failures;
    const auto concurrent_calls = [&](std::size_t index) {
        try {
            for (int round = 0; round < 8; ++round)
                require(same(repeated_resolver.resolve({&shared_read}, repeated_profiles), shared_modes.automatic),
                        "concurrent calls must be safe on one shared resolver");
        } catch (...) { concurrent_failures[index] = std::current_exception(); }
    };
    {
        std::jthread first(concurrent_calls, 0);
        std::jthread second(concurrent_calls, 1);
    }
    for (const auto& failure : concurrent_failures) if (failure) std::rethrow_exception(failure);

    // Three read bases make the start/end bonuses common: three matches score
    // 13, one mismatch then two matches scores 8, and a one-base deletion
    // between three matches scores 7. This gives the exact inclusive 5-point
    // boundary and a true 6-point exclusion without soft clipping.
    Data window = data();
    node(window, 10, 3, {{0, 0, 1}});
    node(window, 11, 3, {{1, 0, 1}});
    node(window, 12, 4, {{2, 0, 1}});
    auto window_reference = write_load(root / "window.pcr", window);
    auto matched = dag({{{10, false, 3, 3}}}, {{}});
    auto mismatched = dag({{{11, false, 3, 3}}}, {{}});
    auto* mismatch_mapping = mismatched.mutable_subpath(0)->mutable_path()->mutable_mapping(0);
    mismatch_mapping->clear_edit();
    auto* mismatch = mismatch_mapping->add_edit();
    mismatch->set_from_length(1); mismatch->set_to_length(1); mismatch->set_sequence("C");
    auto* match_suffix = mismatch_mapping->add_edit();
    match_suffix->set_from_length(2); match_suffix->set_to_length(2);
    auto six_below = dag({{{12, false, 4, 3}}}, {{}});
    auto* deletion_mapping = six_below.mutable_subpath(0)->mutable_path()->mutable_mapping(0);
    deletion_mapping->clear_edit();
    auto* match_prefix = deletion_mapping->add_edit();
    match_prefix->set_from_length(1); match_prefix->set_to_length(1);
    auto* deletion = deletion_mapping->add_edit();
    deletion->set_from_length(1); deletion->set_to_length(0);
    match_suffix = deletion_mapping->add_edit();
    match_suffix->set_from_length(2); match_suffix->set_to_length(2);
    const auto deletion_oracle = TextNodeScoreResolver(window_reference, NodeScoreExecution::enumerate_oracle)
                                     .resolve({&six_below}, profiles());
    require(candidate_for(deletion_oracle[0], "C").tier == direct_count::EvidenceTier::exon &&
                candidate_for(deletion_oracle[0], "C").score == 7,
            "one-base reference deletion retains exact exon evidence at score 7");
    const auto window_oracle = TextNodeScoreResolver(window_reference, NodeScoreExecution::enumerate_oracle)
                                   .resolve({&matched, &mismatched, &six_below}, profiles());
    require(window_oracle[0].facts.exact.size() == 2 &&
                candidate_for(window_oracle[0], "A").tier == direct_count::EvidenceTier::exon &&
                candidate_for(window_oracle[0], "A").score == 13 &&
                candidate_for(window_oracle[0], "B").tier == direct_count::EvidenceTier::exon &&
                candidate_for(window_oracle[0], "B").score == 8,
            "inclusive 5-point window retains the lower mismatch but excludes the 6-point deletion route");
    differential(window_reference, {&matched, &mismatched, &six_below}, "inclusive 5/6 score window");

    // At a merge, the score-leading body prefix and lower-scoring exon prefix
    // are distinct DP states. The latter is the only partial-exon winner.
    Data prefix = data();
    node(prefix, 30, 10, {{0, 0, 4}});
    node(prefix, 31, 5, {{0, 0, 1}});
    node(prefix, 32, 5, {{0, 0, 4}});
    auto prefix_reference = write_load(root / "prefix.pcr", prefix);
    auto prefix_merge = dag({{{30, false, 10, 10}}, {{31, false, 5, 5}}, {{32, false, 5, 5}}},
                            {{2}, {2}, {}});
    const auto prefix_oracle = TextNodeScoreResolver(prefix_reference, NodeScoreExecution::enumerate_oracle)
                                   .resolve({&prefix_merge}, profiles());
    require(candidate_for(prefix_oracle[0], direct_count::EvidenceTier::partial_exon).score == 20 &&
                candidate_for(prefix_oracle[0], direct_count::EvidenceTier::body).score == 25,
            "lower-prefix partial-exon route survives threshold-crossing merge");
    differential(prefix_reference, {&prefix_merge}, "lower-prefix partial-exon merge winner");
    TextNodeScoreResolver offset_dp(prefix_reference, NodeScoreExecution::force_sparse);
    const auto offset_result = offset_dp.resolve({&prefix_merge}, profiles());
    require(same(prefix_oracle, offset_result) && offset_dp.stats().read_offset_conflicts > 0,
            "conflicting read offsets are reported while preserving oracle parity");

    // Two raw aliases have identical operations but distinct fixed support IDs.
    // Equal winners union their support; a lower body winner contributes nothing.
    Data support = data();
    support.sets = {{}, {10}, {20}, {30}};
    support.models[1] = {4, 0, 0, 0};
    node(support, 20, 10, {{0, 1, 4}, {1, 2, 4}});
    node(support, 21, 5, {{0, 3, 4}});
    auto support_reference = write_load(root / "support.pcr", support);
    auto tied_support = dag({{{20, false, 10, 10}}}, {{}});
    auto losing_support = dag({{{21, false, 5, 5}}}, {{}});
    const auto support_oracle = TextNodeScoreResolver(support_reference, NodeScoreExecution::enumerate_oracle)
                                    .resolve({&tied_support, &losing_support}, profiles());
    require(support_oracle[0].facts.exact.size() == 1 &&
                support_oracle[0].facts.exact[0].tier == direct_count::EvidenceTier::body &&
                support_oracle[0].facts.exact[0].body_support_units == std::vector<std::string>({"10", "20"}),
            "tied raw aliases union fixed support while lower-scoring support is excluded");
    require(support_oracle[0].facts.gene_fallback.size() == 1,
            "body evidence retains an independent Gene fallback winner");
    differential(support_reference, {&tied_support, &losing_support},
                 "raw-alias support union and losing support exclusion");

    // Grow the read-local model-ID map repeatedly, encounter odd IDs before
    // even IDs, and revisit old IDs after insertion. Bucket order must never
    // become local model order or change the restored aliases.
    Data growth = data();
    const auto alias_model = growth.models.front();
    growth.models.assign(256, alias_model);
    std::vector<Posting> odd_models, all_models;
    for (std::uint32_t id = 0; id < 256; ++id) {
        all_models.push_back({id, 0, 1});
        if (id & 1U) odd_models.push_back({id, 0, 1});
    }
    node(growth, 700, 4, std::move(odd_models));
    node(growth, 701, 7, std::move(all_models));
    auto growth_reference = write_load(root / "model-id-growth.pcr", growth);
    auto growth_read = dag({{{700, false, 4, 4}}, {{701, false, 7, 7}}}, {{1}, {}});
    const auto growth_modes = differential_modes(growth_reference, {&growth_read},
                                                 "model-ID map growth preserves encounter-order aliases");
    require(growth_modes.automatic_stats.raw_models == 256 &&
                growth_modes.automatic[0].result.genes == std::vector<std::string>{"A"},
            "model-ID growth lost or reassigned raw models");

    // Distinct transcript IDs with the same transfers and forward splice mask
    // share automatic work. Their policy IDs and fixed support units remain
    // attached to their original raw models.
    Data cross_tx = data();
    cross_tx.sets = {{}, {10}, {20}};
    node(cross_tx, 60, 5, {{0, 1, 4}, {1, 2, 4}});
    node(cross_tx, 61, 5, {{0, 1, 4}, {1, 2, 4}});
    cross_tx.splice_records = {{60, 61, 0, 2, 0}};
    cross_tx.splice_targets = {4, 5};
    cross_tx.junction_records = {{120, 122, 0, 2, 0}};
    cross_tx.junction_owners = {{4, 1}, {5, 1}};
    auto cross_tx_reference = write_load(root / "cross-tx.pcr", cross_tx);
    auto cross_tx_read = dag({{{60, false, 5, 5}}, {{61, false, 5, 5}}}, {{1}, {}});
    const auto cross_tx_modes = differential_modes(cross_tx_reference, {&cross_tx_read},
                                                   "cross-transcript fixed-support grouping");
    require(cross_tx_modes.automatic_stats.raw_models == 2 &&
                cross_tx_modes.automatic_stats.model_groups == 1 &&
                cross_tx_modes.sparse_stats.model_groups == 2,
            "identical cross-transcript transfers must reduce automatic groups only");
    require(cross_tx_modes.automatic_stats.membership_groups == 1 &&
                cross_tx_modes.automatic_stats.model_visits == 2 &&
                cross_tx_modes.automatic_stats.raw_model_transfers == 4 &&
                cross_tx_modes.sparse_stats.model_visits == 4,
            "early membership quotient must avoid raw alias piece construction");
    require(cross_tx_modes.automatic[0].facts.exact.size() == 2 &&
                candidate_for(cross_tx_modes.automatic[0], "A").tier == direct_count::EvidenceTier::body &&
                candidate_for(cross_tx_modes.automatic[0], "A").body_support_units == std::vector<std::string>({"10"}) &&
                candidate_for(cross_tx_modes.automatic[0], "B").tier == direct_count::EvidenceTier::body &&
                candidate_for(cross_tx_modes.automatic[0], "B").body_support_units == std::vector<std::string>({"20"}),
            "cross-transcript grouping restores raw policies and fixed support IDs");

    // No splice effect and an explicit 11 mask are equivalent transfers, so
    // they coalesce despite their distinct transcript identities.
    Data absent_mask3 = data();
    node(absent_mask3, 62, 5, {{0, 0, 1}, {1, 0, 1}});
    node(absent_mask3, 63, 5, {{0, 0, 1}, {1, 0, 1}});
    absent_mask3.splice_records = {{62, 63, 0, 1, 0}};
    absent_mask3.splice_targets = {5};
    absent_mask3.junction_records = {{124, 126, 0, 1, 0}};
    absent_mask3.junction_owners = {{5, 3}};
    auto absent_mask3_reference = write_load(root / "absent-mask3.pcr", absent_mask3);
    auto absent_mask3_read = dag({{{62, false, 5, 5}}, {{63, false, 5, 5}}}, {{1}, {}});
    const auto absent_mask3_modes = differential_modes(absent_mask3_reference, {&absent_mask3_read},
                                                       "absent and explicit mask-3 splice grouping");
    require(absent_mask3_modes.automatic_stats.model_groups == 1 &&
                absent_mask3_modes.sparse_stats.model_groups == 2,
            "absent and explicit mask-3 splice effects must share automatic work");

    // A zero-reference mapping contributes support-set identity but no transfer.
    // It partitions fixed and variable support modes even when their current
    // operation vectors are otherwise identical.
    Data support_mode = data();
    support_mode.sets = {{}, {10}, {20}};
    node(support_mode, 80, 5, {{0, 1, 4}, {1, 1, 4}});
    node(support_mode, 81, 1, {{1, 2, 4}});
    auto support_mode_reference = write_load(root / "support-mode.pcr", support_mode);
    auto support_mode_read = dag({{{80, false, 5, 5}}, {{81, false, 0, 1}}}, {{1}, {}});
    const auto support_mode_modes = differential_modes(support_mode_reference, {&support_mode_read},
                                                       "fixed versus variable support-mode grouping");
    require(support_mode_modes.automatic_stats.model_groups == 2 &&
                support_mode_modes.sparse_stats.model_groups == 2,
            "fixed and variable support modes must not share automatic work");
    require(candidate_for(support_mode_modes.automatic[0], "A").body_support_units == std::vector<std::string>({"10"}) &&
                candidate_for(support_mode_modes.automatic[0], "B").body_support_units == std::vector<std::string>({"10"}),
            "unused variable support does not alter the raw winning support unit");

    // Both raw models have the same normalized slot pattern, but multiple
    // support identities. Keep them singleton rather than assuming a fixed
    // support payload can be reconstructed from body_seen alone.
    Data mixed = data();
    mixed.sets = {{}, {10}, {20}, {30}};
    node(mixed, 82, 5, {{0, 1, 4}, {1, 2, 4}});
    node(mixed, 83, 1, {{0, 3, 4}, {1, 3, 4}});
    auto mixed_reference = write_load(root / "mixed-support.pcr", mixed);
    auto mixed_read = dag({{{82, false, 5, 5}}, {{83, false, 0, 1}}}, {{1}, {}});
    const auto mixed_modes = differential_modes(mixed_reference, {&mixed_read},
                                                 "mixed-support singleton membership quotient");
    require(mixed_modes.automatic_stats.membership_groups == 2 &&
                mixed_modes.automatic_stats.model_groups == 2 &&
                mixed_modes.automatic_stats.frontier_compactions == 0,
            "variable support models must retain singleton construction and DP");

    // Run-compressing support IDs must retain A,A,B,A across nodes/records,
    // without suppressing opposite body flags, repeated mappings or insertions.
    Data recurring_support = data();
    recurring_support.sets = {{}, {10}, {20}};
    node(recurring_support, 90, 5, {{0, 1, 4}});
    node(recurring_support, 91, 11, {{0, 1, 8}});
    node(recurring_support, 92, 5, {{0, 2, 4}});
    node(recurring_support, 93, 5, {{0, 1, 4}});
    auto recurring_reference = write_load(root / "recurring-support.pcr", recurring_support);
    auto recurring_first = dag({{{90, false, 5, 5}, {91, true, 11, 11},
                                  {92, false, 5, 5}, {90, false, 0, 1}}}, {{}});
    auto recurring_second = dag({{{93, false, 5, 5}}}, {{}});
    const auto recurring_modes = differential_modes(recurring_reference,
        {&recurring_first, &recurring_second}, "adjacent and recurring mixed support IDs");
    const std::string recurring_gene = "A";
    for (const auto& assignment : recurring_modes.automatic) {
        const auto& candidate = candidate_for(assignment, recurring_gene);
        require(candidate.body_support_units == std::vector<std::string>({"10", "20"}) &&
                    candidate.tier == direct_count::EvidenceTier::body &&
                    candidate.strand == direct_count::EvidenceStrand::forward,
                "support dedup must retain mixed support, strand and body eligibility");
    }
    require(recurring_modes.automatic_stats.posting_rows_decoded == 4 &&
                recurring_modes.automatic_stats.incidence_list_rows == 4 &&
                recurring_modes.automatic_stats.incidence_node_rows == 4,
            "support dedup must not remove node postings or record memberships");

    Data across_records = data();
    across_records.sets = {{}, {10}, {20}, {30}};
    node(across_records, 84, 5, {{0, 1, 4}, {1, 2, 4}});
    node(across_records, 85, 5, {{0, 3, 4}, {1, 3, 4}});
    auto across_reference = write_load(root / "support-across-records.pcr", across_records);
    auto across_first = dag({{{84, false, 5, 5}}}, {{}});
    auto across_second = dag({{{85, false, 5, 5}}}, {{}});
    const auto across_modes = differential_modes(across_reference, {&across_first, &across_second},
                                                  "read-group-wide mixed support across records");
    require(across_modes.automatic_stats.membership_groups == 4 &&
                across_modes.automatic_stats.model_groups == 4 &&
                candidate_for(across_modes.automatic[0], "A").body_support_units ==
                    std::vector<std::string>({"10", "30"}) &&
                candidate_for(across_modes.automatic[0], "B").body_support_units ==
                    std::vector<std::string>({"20", "30"}),
            "every record must use the complete QNAME support mode and tied support union");

    // Repeated body credits must coalesce per piece without retaining support
    // slots into the next record.
    Data repeated_credits = data();
    repeated_credits.sets = {{}, {10}, {20}};
    node(repeated_credits, 88, 5, {{0, 1, 4}, {1, 2, 4}});
    auto repeated_credits_reference = write_load(root / "repeated-credits.pcr", repeated_credits);
    auto repeated_credit_first = dag({{{88, false, 5, 5}, {88, false, 5, 5}}}, {{}});
    auto repeated_credit_second = dag({{{88, true, 5, 5}}}, {{}});
    const auto repeated_credit_modes = differential_modes(
        repeated_credits_reference, {&repeated_credit_first, &repeated_credit_second},
        "repeated body credits retain support across records");
    require(candidate_for(repeated_credit_modes.automatic[0], "A").body_support_units ==
                std::vector<std::string>({"10"}) &&
                candidate_for(repeated_credit_modes.automatic[0], "B").body_support_units ==
                    std::vector<std::string>({"20"}) &&
                candidate_for(repeated_credit_modes.automatic[1], "A").body_support_units ==
                    std::vector<std::string>({"10"}) &&
                candidate_for(repeated_credit_modes.automatic[1], "B").body_support_units ==
                    std::vector<std::string>({"20"}),
            "repeated credits and a second record retain only their fixed support IDs");

    // Repeated annotation-free nodes cannot suppress later body evidence, and
    // the read-local posting descriptors must preserve the shared support ID.
    Data repeated = data();
    repeated.sets = {{}, {10}};
    node(repeated, 86, 1, {});
    node(repeated, 87, 5, {{0, 1, 4}});
    auto repeated_reference = write_load(root / "repeated-node.pcr", repeated);
    auto repeated_first = dag({{{86, false, 1, 1}, {86, false, 1, 1}, {87, false, 5, 5}}}, {{}});
    auto repeated_second = dag({{{87, true, 5, 5}, {86, false, 1, 1}}}, {{}});
    const auto repeated_modes = differential_modes(
        repeated_reference, {&repeated_first, &repeated_second},
        "repeated nodes and annotation-free prefix preserve body support");
    require(candidate_for(repeated_modes.automatic[0], "A").body_support_units ==
                std::vector<std::string>({"10"}) &&
                candidate_for(repeated_modes.automatic[1], "A").body_support_units ==
                    std::vector<std::string>({"10"}),
            "repeated nodes retain body support for both profiles");

    // Winner rows are materialized only after a real score. Models 0 and 2
    // tie below zero across records and share one transcript/policy winner;
    // model 1 is exactly five points lower under a distinct policy for that
    // transcript, and model 3 is observed only through an insertion.
    Data late_winners = data();
    late_winners.sets = {{}, {10}, {20}, {30}};
    late_winners.models[1] = {4, 1, 1, 0};
    late_winners.models[2] = {4, 0, 0, 0};
    node(late_winners, 88, 4, {{0, 1, 4}});
    node(late_winners, 89, 4, {{2, 2, 4}});
    node(late_winners, 90, 4, {{1, 3, 4}});
    node(late_winners, 91, 1, {{3, 3, 4}});
    auto late_reference = write_load(root / "late-winners.pcr", late_winners);
    auto tied_first = dag({{{88, false, 4, 4}}}, {{}});
    auto tied_second = dag({{{89, false, 4, 4}}}, {{}});
    auto lower_policy = dag({{{90, false, 4, 4}}}, {{}});
    auto never_scored = dag({{{91, false, 0, 1}}}, {{}});
    const auto set_mismatches = [](vg::MultipathAlignment& alignment, int mismatches) {
        auto* mapping = alignment.mutable_subpath(0)->mutable_path()->mutable_mapping(0);
        mapping->clear_edit();
        auto* mismatch = mapping->add_edit();
        mismatch->set_from_length(mismatches);
        mismatch->set_to_length(mismatches);
        mismatch->set_sequence(std::string(static_cast<std::size_t>(mismatches), 'C'));
        if (mismatches != 4) {
            auto* match = mapping->add_edit();
            match->set_from_length(4 - mismatches);
            match->set_to_length(4 - mismatches);
        }
    };
    set_mismatches(tied_first, 3);
    set_mismatches(tied_second, 3);
    set_mismatches(lower_policy, 4);
    TextNodeScoreResolver late_oracle(late_reference, NodeScoreExecution::enumerate_oracle);
    const auto tied_only = late_oracle.resolve({&tied_first}, profiles());
    const auto lower_only = late_oracle.resolve({&lower_policy}, profiles());
    require(candidate_for(tied_only[0], "A").score < 0 &&
                candidate_for(lower_only[0], "B").score ==
                    candidate_for(tied_only[0], "A").score - 5,
            "late-winner fixture must exercise negative and inclusive-window scores");
    const auto late_modes = differential_modes(
        late_reference, {&tied_first, &tied_second, &lower_policy, &never_scored},
        "late winner materialization must match eager oracle");
    require(late_modes.automatic[0].facts.exact.size() == 1 &&
                candidate_for(late_modes.automatic[0], "A").body_support_units ==
                    std::vector<std::string>({"10", "20"}),
            "late winners must retain transcript maxima and cross-record support ties");

    // Different physical memberships can produce the same piece transfer.
    // Rejoin only after scoring, then restore both policies and support IDs.
    Data rejoined = data();
    rejoined.sets = {{}, {10}, {20}};
    node(rejoined, 86, 5, {{0, 1, 4}});
    node(rejoined, 87, 5, {{1, 2, 4}});
    auto rejoined_reference = write_load(root / "rejoined-operations.pcr", rejoined);
    auto rejoined_read = dag({{{86, false, 5, 5}, {87, false, 5, 5}}}, {{}});
    const auto rejoined_modes = differential_modes(rejoined_reference, {&rejoined_read},
                                                    "second quotient restores all fixed-support aliases");
    require(rejoined_modes.automatic_stats.membership_groups == 2 &&
                rejoined_modes.automatic_stats.model_groups == 1 &&
                candidate_for(rejoined_modes.automatic[0], "A").body_support_units ==
                    std::vector<std::string>({"10"}) &&
                candidate_for(rejoined_modes.automatic[0], "B").body_support_units ==
                    std::vector<std::string>({"20"}),
            "identical operations must rejoin without losing raw policy or fixed support");

    // A splice effect crossed only after an empty and insertion-only bridge
    // still splits otherwise equal transfers: A permits forward traversal,
    // while B is vetoed on that route.
    Data bridge = data();
    node(bridge, 70, 5, {{0, 0, 1}, {1, 0, 1}});
    node(bridge, 71, 1, {});
    node(bridge, 72, 5, {{0, 0, 1}, {1, 0, 1}});
    bridge.splice_records = {{70, 72, 0, 2, 0}};
    bridge.splice_targets = {4, 5};
    bridge.junction_records = {{140, 144, 0, 2, 0}};
    bridge.junction_owners = {{4, 1}, {5, 2}};
    auto bridge_reference = write_load(root / "bridge-mask.pcr", bridge);
    auto bridge_read = dag({{{70, false, 5, 5}}, {}, {{71, false, 0, 1}}, {{72, false, 5, 5}}},
                           {{1}, {2}, {3}, {}});
    const auto bridge_modes = differential_modes(bridge_reference, {&bridge_read},
                                                 "empty and insertion bridge splice split");
    require(bridge_modes.automatic_stats.model_groups == 2 && bridge_modes.sparse_stats.model_groups == 2,
            "reachable bridge mask differences must split automatic groups");
    require(bridge_modes.automatic[0].facts.exact.size() == 1 &&
                bridge_modes.automatic[0].facts.exact.front().gene == "A" &&
                bridge_modes.automatic[0].result.genes == std::vector<std::string>({"A"}),
            "bridge splice mask preserves the allowed candidate and vetoes the other");

    // Annotation and Gene fallback pools are independent. Two prefixes merge
    // through a common exon and unannotated terminal: A has the best P score,
    // whereas A has the best body-only Gene score on a different route. The
    // shared suffix forces the DP to merge distinct prefix states rather than
    // select one globally.
    Data independent = data();
    node(independent, 40, 5, {{0, 0, 1}});
    node(independent, 41, 5, {{0, 0, 4}});
    node(independent, 42, 5, {{0, 0, 1}});
    node(independent, 43, 5, {{0, 0, 4}});
    node(independent, 44, 1, {{0, 0, 1}});
    node(independent, 45, 1, {});
    auto independent_reference = write_load(root / "independent.pcr", independent);
    auto independent_read = dag({{{40, false, 5, 5}, {41, false, 5, 5}},
                                 {{42, false, 5, 5}, {43, false, 5, 5}},
                                 {{44, false, 1, 1}}, {{45, false, 1, 1}}},
                                {{2}, {2}, {3}, {}});
    auto* a_body = independent_read.mutable_subpath(0)->mutable_path()->mutable_mapping(1);
    a_body->clear_edit();
    auto* a_match = a_body->add_edit(); a_match->set_from_length(1); a_match->set_to_length(1);
    auto* a_mismatch = a_body->add_edit(); a_mismatch->set_from_length(4); a_mismatch->set_to_length(4);
    a_mismatch->set_sequence("CCCC");
    independent_read.mutable_subpath(1)->mutable_path()->mutable_mapping(0)->mutable_edit(0)->set_sequence("CCCCC");
    const auto independent_oracle = TextNodeScoreResolver(independent_reference, NodeScoreExecution::enumerate_oracle)
                                        .resolve({&independent_read}, profiles());
    require(candidate_for(independent_oracle[0], "A").tier == direct_count::EvidenceTier::partial_exon &&
                candidate_for(independent_oracle[0], "A").score == -4 &&
                independent_oracle[0].facts.gene_fallback.size() == 1 &&
                independent_oracle[0].facts.gene_fallback[0].gene == "A" &&
                independent_oracle[0].facts.gene_fallback[0].score == 5,
            "annotation winner and independent Gene fallback winner remain separate");
    differential(independent_reference, {&independent_read}, "independent annotation and Gene winners");

    // Deterministic small topological DAGs: bounded route counts make the independent
    // enumerator practical. The fixed cycle covers exon, body, and dual flags
    // plus support-set IDs; deliberately conflicting offsets remain a parity
    // regression, reported rather than rejected by the resolver.
    std::mt19937 rng(0x5eedu);
    constexpr std::array<std::uint32_t, 6> random_flags = {1, 2, 4, 8, 5, 10};
    for (int trial = 0; trial != 256; ++trial) {
        Data random = data();
        random.sets = {{}, {10}, {20}, {30}};
        constexpr int count = 6;
        for (int i = 0; i != count; ++i) {
            std::vector<Posting> postings;
            if (trial < static_cast<int>(random_flags.size()) || (rng() % 5) != 0) {
                const auto flags = random_flags[static_cast<std::size_t>(trial + i) % random_flags.size()];
                const auto support_set = static_cast<std::uint32_t>(flags & 12U ? (trial + i) % 4 : 0);
                postings.push_back({static_cast<std::uint32_t>(rng() % 4), support_set, flags});
            }
            node(random, 100 + i, 10, std::move(postings));
        }
        auto reference = write_load(root / ("random-" + std::to_string(trial) + ".pcr"), random);
        std::vector<std::vector<std::tuple<std::uint64_t, bool, int, int>>> pieces;
        std::vector<std::vector<int>> next(count);
        for (int i = 0; i != count; ++i) {
            pieces.push_back({{static_cast<std::uint64_t>(100 + i), (rng() & 1u) != 0,
                               3 + static_cast<int>(rng() % 8), 3 + static_cast<int>(rng() % 8)}});
            if (i + 1 < count) next[i].push_back(i + 1);
            if (i + 2 < count && (rng() & 1u)) next[i].push_back(i + 2);
        }
        auto alignment = dag(pieces, next);
        differential(reference, {&alignment}, "deterministic random bounded DAG differential");
    }

    // Bounded seeded cross-transcript cases combine fixed-support aliases with
    // five reachable splice edges, including an empty/insertion bridge. Each
    // trial gives the two transcripts at least one distinct mask, so automatic
    // execution must retain two groups while matching the frozen oracle.
    std::mt19937 cross_tx_rng(0xc0a55eu);
    for (int trial = 0; trial != 64; ++trial) {
        Data random = data();
        random.sets = {{}, {10}, {20}, {30}};
        const auto add_annotated = [&](std::uint64_t id, int ordinal) {
            const auto flags = random_flags[static_cast<std::size_t>(trial + ordinal) % random_flags.size()];
            const auto support_a = static_cast<std::uint32_t>(flags & 12U ? 1 : 0);
            const auto support_b = static_cast<std::uint32_t>(flags & 12U ? 2 : 0);
            node(random, id, 3, {{0, support_a, flags}, {1, support_b, flags}});
        };
        add_annotated(800, 0);
        node(random, 801, 1, {});
        add_annotated(802, 1);
        add_annotated(803, 2);
        add_annotated(804, 3);
        add_annotated(805, 4);
        const auto add_splice = [&](std::uint64_t from, std::uint64_t to, int ordinal) {
            const auto target_first = static_cast<std::uint64_t>(random.splice_targets.size());
            random.splice_targets.push_back(4);
            random.splice_targets.push_back(5);
            random.splice_records.push_back({from, to, target_first, 2, 0});
            const auto owner_first = static_cast<std::uint64_t>(random.junction_owners.size());
            const auto first_mask = static_cast<std::uint32_t>(1 + cross_tx_rng() % 3);
            const auto second_mask = static_cast<std::uint32_t>((trial + ordinal) & 1 ? first_mask :
                                                                 first_mask % 3 + 1);
            random.junction_owners.push_back({4, first_mask});
            random.junction_owners.push_back({5, second_mask});
            random.junction_records.push_back({from << 1, to << 1, owner_first, 2, 0});
        };
        add_splice(800, 802, 0);
        add_splice(802, 803, 1);
        add_splice(802, 804, 2);
        add_splice(803, 805, 3);
        add_splice(804, 805, 4);
        auto reference = write_load(root / ("cross-tx-random-" + std::to_string(trial) + ".pcr"), random);
        auto alignment = dag({{{800, false, 3, 3}}, {}, {{801, false, 0, 1}}, {{802, false, 3, 3}},
                              {{803, false, 3, 3}}, {{804, false, 3, 3}}, {{805, false, 3, 3}}},
                             {{1}, {2}, {3}, {4, 5}, {6}, {6}, {}});
        const auto modes = differential_modes(reference, {&alignment},
                                              "seeded cross-transcript splice differential");
        require(modes.automatic_stats.model_groups == 2 && modes.sparse_stats.model_groups == 2,
                "different reachable splice masks must split seeded cross-transcript groups");
    }

    // Coherent equal-read-length diamonds: every branch has the same 10-base
    // piece, so each merge has a single well-defined read offset.
    Data coherent = data();
    for (int i = 0; i != 14; ++i) node(coherent, 500 + i, 10, {{0, 0, 1}});
    auto coherent_reference = write_load(root / "coherent.pcr", coherent);
    std::vector<std::vector<std::tuple<std::uint64_t, bool, int, int>>> coherent_pieces;
    std::vector<std::vector<int>> coherent_next(14);
    for (int i = 0; i != 14; ++i) coherent_pieces.push_back({{static_cast<std::uint64_t>(500 + i), false, 10, 10}});
    coherent_next[0] = {1, 2};
    for (int layer = 0; layer != 6; ++layer) {
        const int left = 1 + 2 * layer, right = left + 1;
        const int next_left = layer == 5 ? 13 : left + 2;
        const int next_right = layer == 5 ? 13 : right + 2;
        coherent_next[left] = {next_left, next_right};
        coherent_next[right] = {next_left, next_right};
    }
    auto coherent_alignment = dag(coherent_pieces, coherent_next);
    differential(coherent_reference, {&coherent_alignment}, "coherent equal-length diamond differential");

    // Unequal reference lengths create distinct positive balances at merges,
    // although every completion is exonic/sense. Their exact magnitudes cannot
    // affect any terminal pool, including the independent Gene objective.
    Data varying = data();
    constexpr int varying_layers = 10, varying_nodes = 2 * varying_layers + 2;
    std::vector<std::vector<std::tuple<std::uint64_t, bool, int, int>>> varying_paths;
    std::vector<std::vector<int>> varying_next(varying_nodes);
    for (int i = 0; i < varying_nodes; ++i) {
        const int bases = i > 0 && i < varying_nodes - 1 && i % 2 == 0 ? 2 : 1;
        node(varying, 2000 + i, bases, {{0, 0, 5}});
        varying_paths.push_back({{static_cast<std::uint64_t>(2000 + i), false, bases, 1}});
    }
    varying_next[0] = {1, 2};
    for (int layer = 0; layer < varying_layers; ++layer) {
        const int left = 1 + 2 * layer, right = left + 1;
        const auto successors = layer + 1 == varying_layers ? std::vector<int>{varying_nodes - 1}
                                                            : std::vector<int>{left + 2, right + 2};
        varying_next[left] = successors;
        varying_next[right] = successors;
    }
    auto varying_reference = write_load(root / "varying-positive-balances.pcr", varying);
    auto varying_alignment = dag(varying_paths, varying_next);
    const auto varying_modes = differential_modes(varying_reference, {&varying_alignment},
        "threshold saturation must preserve variable-length exon/Gene winners and support");
    TextNodeScoreResolver varying_map(varying_reference, NodeScoreExecution::force_map);
    require(same(varying_modes.automatic, varying_map.resolve({&varying_alignment}, profiles())) &&
                varying_modes.automatic_stats.balance_saturated_groups > 0 &&
                varying_modes.automatic_stats.balance_unsaturated_groups == 0 &&
                varying_map.stats().dp_states > 3 * varying_modes.automatic_stats.dp_states,
            "threshold saturation must merge provably equivalent positive balances");

    // DP-only combinatorial diamond: 2^32 routes, but one state per node/model
    // group. Enumeration is intentionally not invoked on this fixture.
    Data fanout = data();
    constexpr int layers = 32;
    constexpr int fanout_nodes = 2 * layers + 2;
    for (int i = 0; i != fanout_nodes; ++i) node(fanout, 1000 + i, 10, {{0, 0, 1}});
    auto fanout_reference = write_load(root / "fanout.pcr", fanout);
    std::vector<std::vector<std::tuple<std::uint64_t, bool, int, int>>> pieces;
    std::vector<std::vector<int>> next(fanout_nodes);
    for (int i = 0; i != fanout_nodes; ++i) pieces.push_back({{static_cast<std::uint64_t>(1000 + i), false, 10, 10}});
    next[0] = {1, 2};
    for (int layer = 0; layer != layers; ++layer) {
        const int left = 1 + 2 * layer, right = left + 1;
        const int next_left = layer + 1 == layers ? fanout_nodes - 1 : left + 2;
        const int next_right = layer + 1 == layers ? fanout_nodes - 1 : right + 2;
        next[left] = {next_left, next_right};
        next[right] = {next_left, next_right};
    }
    auto fanout_alignment = dag(pieces, next);
    const auto profile_set = profiles();
    TextNodeScoreResolver dp(fanout_reference, NodeScoreExecution::automatic);
    static_cast<void>(dp.resolve({&fanout_alignment}, profile_set));
    const auto stats = dp.stats();
    require(stats.complete_alignments >= (UINT64_C(1) << 32) && stats.dp_states < 1000 &&
                stats.dp_transitions >= stats.dp_states,
            "sparse DP must score exponential fanout without enumerating complete alignments");

    std::filesystem::remove_all(root);
    std::cout << "node score DP differential checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
