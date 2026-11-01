#include "native_count_index.hpp"
#include "native_count_reference.hpp"
#include "node_score_resolver.hpp"
#include "bounded_sharded_cache.hpp"
#include "count_assignment_signature.hpp"
#include "count_reference.hpp"
#include "count_support_storage.hpp"

#include <gbwt/dynamic_gbwt.h>
#include <gbwt/fast_locate.h>
#include <gbwt/gbwt.h>
#include <xg.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <numeric>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <thread>
#include <utility>
#include <vector>

namespace {

using pancollapse::NativeSource;
using pancollapse::BoundedShardedCache;
using pancollapse::count_support::SupportIds;
using pancollapse::count_support::SupportSet;
using pancollapse::prepared_count::assignment_graph_signature;
using pancollapse::count_reference::EdgePosting;
using pancollapse::count_reference::JunctionOwner;
using pancollapse::count_reference::Posting;
using pancollapse::count_reference::ScoringPosting;
using pancollapse::native_count::Data;
using pancollapse::native_count::PathRecord;

constexpr std::uint32_t kExonForward = 1;
constexpr std::uint32_t kExonReverse = 2;
constexpr std::uint32_t kBodyForward = 4;
constexpr std::uint32_t kOrdinaryBodyEdge = 2;
constexpr std::uint32_t kAlongTranscript = 1;
constexpr std::uint32_t kReverseComplement = 2;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

std::uint64_t oriented(std::uint64_t node, bool reverse) {
    return (node << 1) | static_cast<std::uint64_t>(reverse);
}

const Posting& posting_for(std::span<const Posting> postings, std::uint32_t model) {
    const auto found = std::find_if(postings.begin(), postings.end(), [&](const Posting& posting) {
        return posting.model == model;
    });
    require(found != postings.end(), "expected native node posting");
    return *found;
}

const EdgePosting& edge_for(std::span<const EdgePosting> postings, std::uint32_t model) {
    const auto found = std::find_if(postings.begin(), postings.end(), [&](const EdgePosting& posting) {
        return posting.model == model;
    });
    require(found != postings.end(), "expected native edge posting");
    return *found;
}

const ScoringPosting& scoring_for(std::span<const ScoringPosting> postings, std::uint32_t row) {
    const auto found = std::find_if(postings.begin(), postings.end(), [&](const ScoringPosting& posting) {
        return posting.class_id == row;
    });
    require(found != postings.end(), "expected native scoring posting");
    return *found;
}

const JunctionOwner& owner_for(std::span<const JunctionOwner> owners, std::uint32_t target) {
    const auto found = std::find_if(owners.begin(), owners.end(), [&](const JunctionOwner& owner) {
        return owner.transcript_string == target;
    });
    require(found != owners.end(), "expected native junction owner");
    return *found;
}

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("pancollapse-native-query-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

PathRecord path_record(std::uint32_t transcript, std::uint32_t policy, std::uint32_t support,
                       std::uint32_t flags, std::uint32_t strand) {
    PathRecord result;
    result.transcript_string = transcript;
    result.policy = policy;
    result.support_set = support;
    result.flags = flags;
    result.strand = strand;
    return result;
}

void write_xg(const std::filesystem::path& path) {
    xg::XG graph;
    graph.from_enumerators(
        [](const auto& emit) {
            for (handlegraph::nid_t id = 1; id <= 21; ++id) emit("A", id);
        },
        [](const auto&) {},
        [](const auto&) {},
        false);
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output), "cannot create native-query XG fixture");
    graph.serialize(output);
    require(static_cast<bool>(output), "cannot write native-query XG fixture");
}

gbwt::GBWT write_gbwt(const std::filesystem::path& path,
                      const std::vector<std::vector<std::pair<std::uint64_t, bool>>>& walks) {
    gbwt::GBWTBuilder builder(8, 1024, 1);
    for (const auto& source_walk : walks) {
        gbwt::vector_type walk;
        walk.reserve(source_walk.size());
        for (const auto& [node, reverse] : source_walk) {
            walk.push_back(gbwt::Node::encode(node, reverse));
        }
        builder.insert(walk, true);
    }
    builder.finish();
    gbwt::GBWT index(builder.index);
    index.addMetadata();
    index.metadata.setSamples({"fixture-sample"});
    index.metadata.setContigs({"fixture-contig"});
    index.metadata.setHaplotypes(1);
    for (std::size_t path_id = 0; path_id < walks.size(); ++path_id) {
        index.metadata.addPath(0, 0, 0, path_id);
    }
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output), "cannot create native-query GBWT fixture");
    index.serialize(output);
    require(static_cast<bool>(output), "cannot write native-query GBWT fixture");
    return index;
}

void write_r_index(const std::filesystem::path& path, const gbwt::GBWT& index) {
    gbwt::FastLocate locate(index);
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output), "cannot create native-query r-index fixture");
    locate.serialize(output);
    require(static_cast<bool>(output), "cannot write native-query r-index fixture");
}

struct GbwtLocateSnapshot {
    gbwt::size_type sequences = 0;
    gbwt::size_type effective = 0;
    bool bidirectional = false;
    bool contains = false;
    bool empty = true;
    bool find_empty = true;
    gbwt::size_type first = gbwt::FastLocate::NO_POSITION;
    std::vector<gbwt::size_type> packed_sa;
    std::vector<gbwt::size_type> located_sequences;
    bool operator==(const GbwtLocateSnapshot&) const = default;
};

GbwtLocateSnapshot snapshot_gbwt(const gbwt::GBWT& index, gbwt::FastLocate& locate) {
    const auto first_node = gbwt::Node::encode(1, false);
    const std::array<gbwt::node_type, 2> query = {
        first_node, gbwt::Node::encode(2, false)};
    GbwtLocateSnapshot result;
    result.sequences = index.sequences();
    result.effective = index.effective();
    result.bidirectional = index.bidirectional();
    result.contains = index.contains(first_node);
    result.empty = index.empty(first_node);
    result.packed_sa = locate.decompressSA(first_node);
    const auto state = locate.find(query.begin(), query.end(), result.first);
    result.find_empty = state.empty();
    if (!state.empty()) result.located_sequences = locate.locate(state, result.first);
    return result;
}

vg::MultipathAlignment read(const std::vector<std::pair<std::uint64_t, bool>>& nodes) {
    vg::MultipathAlignment result;
    result.set_sequence(std::string(nodes.size(), 'A'));
    auto* subpath = result.add_subpath();
    subpath->set_score(10);
    for (const auto [id, reverse] : nodes) {
        auto* mapping = subpath->mutable_path()->add_mapping();
        mapping->mutable_position()->set_node_id(id);
        mapping->mutable_position()->set_is_reverse(reverse);
        auto* edit = mapping->add_edit();
        edit->set_from_length(1);
        edit->set_to_length(1);
    }
    return result;
}

vg::MultipathAlignment split_read(std::uint64_t first, bool first_reverse,
                                  std::uint64_t second, bool second_reverse) {
    vg::MultipathAlignment result;
    result.set_sequence("AA");
    auto* left = result.add_subpath();
    left->set_score(10);
    left->add_next(1);
    auto* left_mapping = left->mutable_path()->add_mapping();
    left_mapping->mutable_position()->set_node_id(first);
    left_mapping->mutable_position()->set_is_reverse(first_reverse);
    auto* left_edit = left_mapping->add_edit();
    left_edit->set_from_length(1);
    left_edit->set_to_length(1);
    auto* right = result.add_subpath();
    right->set_score(10);
    auto* right_mapping = right->mutable_path()->add_mapping();
    right_mapping->mutable_position()->set_node_id(second);
    right_mapping->mutable_position()->set_is_reverse(second_reverse);
    auto* right_edit = right_mapping->add_edit();
    right_edit->set_from_length(1);
    right_edit->set_to_length(1);
    return result;
}

void append_mapping(vg::Subpath& subpath, std::uint64_t node, bool reverse,
                    std::uint32_t reference_length = 1, std::uint32_t offset = 0) {
    auto* mapping = subpath.mutable_path()->add_mapping();
    mapping->mutable_position()->set_node_id(node);
    mapping->mutable_position()->set_is_reverse(reverse);
    mapping->mutable_position()->set_offset(offset);
    auto* edit = mapping->add_edit();
    edit->set_from_length(reference_length);
    edit->set_to_length(1);
}

vg::MultipathAlignment scored_connection_read() {
    vg::MultipathAlignment result;
    result.set_sequence("AA");
    auto* left = result.add_subpath();
    left->set_score(10);
    append_mapping(*left, 11, false);
    auto* connection = left->add_connection();
    connection->set_next(1);
    connection->set_score(3);
    auto* right = result.add_subpath();
    right->set_score(10);
    append_mapping(*right, 12, false);
    return result;
}

struct ConstantStringHash {
    std::size_t operator()(const std::string&) const noexcept { return 0; }
};

struct FirstCharacterHash {
    std::size_t operator()(const std::string& value) const noexcept {
        return value.empty() ? 0 : static_cast<unsigned char>(value.front());
    }
};

void require_cache_signature_and_support_storage() {
    const std::array<std::pair<std::uint64_t, std::size_t>, 6> varint_boundaries = {{
        {0, 1}, {127, 1}, {128, 2}, {16383, 2}, {16384, 3},
        {std::numeric_limits<std::uint64_t>::max(), 10}}};
    for (const auto [value, expected_size] : varint_boundaries) {
        std::string encoded;
        pancollapse::prepared_count::detail::append_assignment_signature_integer(encoded, value);
        require(encoded.size() == expected_size &&
                    pancollapse::prepared_count::detail::assignment_signature_varint_size(value) == expected_size,
                "assignment signature varints must use canonical boundary widths");
    }

    using CollisionCache = BoundedShardedCache<std::string, int, 1, ConstantStringHash>;
    CollisionCache collision(8);
    require(*collision.get_or_build("left", [] { return 1; }) == 1,
            "bounded cache must retain the first collision key");
    require(*collision.get_or_build("right", [] { return 2; }) == 2,
            "bounded cache must distinguish colliding full keys");
    require(*collision.get_or_build("left", [] { return 99; }) == 1,
            "bounded cache must reuse the exact collision key");
    require(*collision.get_or_build("", [] { return 3; }) == 3,
            "bounded cache must support an empty key");
    require(collision.entries() == 3 && collision.hits() == 1 && collision.misses() == 3,
            "bounded cache collision counters must count exact-key reuse");
    require(collision.approximate_bytes([](const int&) { return sizeof(int); }) != 0,
            "bounded cache approximate bytes must include retained entries");

    using ByteCache = BoundedShardedCache<std::string, std::string, 2, FirstCharacterHash>;
    const auto value_bytes = [](const std::string& value) { return value.capacity() + 1; };
    const std::string first_key = "a-key-with-heap-storage";
    const std::string second_key = "b-key-with-heap-storage";
    ByteCache probe(8, std::numeric_limits<std::size_t>::max(), value_bytes);
    static_cast<void>(probe.get_or_build(first_key, [] { return std::string("payload"); }));
    const std::size_t one_entry_charge = probe.charged_bytes();
    require(one_entry_charge != 0 && one_entry_charge <= std::numeric_limits<std::size_t>::max() / 2,
            "bounded cache charge must cover a finite retained entry");

    ByteCache bytes_bounded(8, one_entry_charge * 2 - 1, value_bytes);
    static_cast<void>(bytes_bounded.get_or_build(first_key, [] { return std::string("payload"); }));
    static_cast<void>(bytes_bounded.get_or_build(second_key, [] { return std::string("payload"); }));
    require(bytes_bounded.entries() == 1 && bytes_bounded.uncached() == 1 &&
                bytes_bounded.charged_bytes() == one_entry_charge,
            "bounded cache byte limit must apply globally across shards");

    ByteCache oversize(8, one_entry_charge - 1, value_bytes);
    static_cast<void>(oversize.get_or_build(first_key, [] { return std::string("payload"); }));
    require(oversize.entries() == 0 && oversize.uncached() == 1 && oversize.charged_bytes() == 0,
            "oversize entries must be returned uncached without reserving bytes");

    bool threw = false;
    try {
        CollisionCache invalid(0);
        static_cast<void>(invalid);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "zero bounded-cache capacity must throw");

    CollisionCache throwing_builder(1);
    threw = false;
    try {
        static_cast<void>(throwing_builder.get_or_build("throw", []() -> int {
            throw std::runtime_error("builder failure");
        }));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw && throwing_builder.entries() == 0 && throwing_builder.misses() == 0,
            "throwing cache builders must not retain or count a value");

    BoundedShardedCache<std::string, int, 1> throwing_sizer(
        1, std::numeric_limits<std::size_t>::max(),
        [](const int&) -> std::size_t { throw std::runtime_error("sizer failure"); });
    threw = false;
    try {
        static_cast<void>(throwing_sizer.get_or_build("throw", [] { return 1; }));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw && throwing_sizer.entries() == 0 && throwing_sizer.misses() == 0,
            "throwing cache size callbacks must not retain or count a value");

    CollisionCache concurrent(8);
    std::atomic<std::size_t> builders{0};
    std::atomic<bool> consistent{true};
    std::vector<std::thread> workers;
    workers.reserve(8);
    for (std::size_t index = 0; index < 8; ++index) {
        workers.emplace_back([&] {
            const auto value = concurrent.get_or_build("concurrent", [&] {
                builders.fetch_add(1, std::memory_order_relaxed);
                return 7;
            });
            if (*value != 7) consistent.store(false, std::memory_order_relaxed);
        });
    }
    for (auto& worker : workers) worker.join();
    require(consistent.load(std::memory_order_relaxed) && builders.load(std::memory_order_relaxed) >= 1 &&
                builders.load(std::memory_order_relaxed) <= workers.size() && concurrent.entries() == 1 &&
                concurrent.hits() + concurrent.misses() == workers.size(),
            "concurrent cache lookups must retain one exact-key value without lock-held builders");

    vg::MultipathAlignment graph = read({{1, false}});
    graph.set_name("first-qname");
    graph.set_quality("!");
    const std::vector<const vg::MultipathAlignment*> empty_records;
    const std::string empty_signature = assignment_graph_signature(empty_records);
    require(!empty_signature.empty() && empty_signature == assignment_graph_signature(empty_records),
            "empty record groups must have a deterministic assignment signature");
    const auto graph_signature = assignment_graph_signature(
        std::vector<const vg::MultipathAlignment*>{&graph});
    require(graph_signature.size() == 14,
            "one-mapping assignment signature must use 14 compact varint bytes, not 112 fixed-width bytes");

    auto molecule_fields = graph;
    molecule_fields.set_name("other-qname");
    molecule_fields.set_quality("I");
    molecule_fields.set_sequence("C");
    require(graph_signature == assignment_graph_signature(
                std::vector<const vg::MultipathAlignment*>{&molecule_fields}),
            "assignment signatures must ignore QNAME, quality, and sequence content");

    auto different_length = graph;
    different_length.set_sequence("AA");
    require(graph_signature != assignment_graph_signature(
                std::vector<const vg::MultipathAlignment*>{&different_length}),
            "assignment signatures must retain whole-read sequence length");
    auto ordered_starts = graph;
    ordered_starts.add_start(0);
    ordered_starts.add_start(1);
    auto reordered_starts = graph;
    reordered_starts.add_start(1);
    reordered_starts.add_start(0);
    require(assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&ordered_starts}) !=
                assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&reordered_starts}),
            "assignment signatures must retain start order");
    auto ignored_subpath_score = graph;
    ignored_subpath_score.mutable_subpath(0)->set_score(11);
    auto different_node = graph;
    different_node.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->
        mutable_position()->set_node_id(2);
    auto different_orientation = graph;
    different_orientation.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->
        mutable_position()->set_is_reverse(true);
    auto different_offset = graph;
    different_offset.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->
        mutable_position()->set_offset(1);
    auto different_from = graph;
    different_from.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->
        mutable_edit(0)->set_from_length(2);
    auto different_to = graph;
    different_to.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->
        mutable_edit(0)->set_to_length(2);
    auto extra_empty_subpath = graph;
    extra_empty_subpath.add_subpath();
    auto extra_zero_base_mapping = graph;
    extra_zero_base_mapping.mutable_subpath(0)->mutable_path()->add_mapping()->
        mutable_position()->set_node_id(1);
    auto nonempty_edit_sequence = graph;
    nonempty_edit_sequence.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->
        mutable_edit(0)->set_sequence("C");
    auto different_nonempty_edit_sequence = nonempty_edit_sequence;
    different_nonempty_edit_sequence.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->
        mutable_edit(0)->set_sequence("G");
    require(graph_signature != assignment_graph_signature(
                std::vector<const vg::MultipathAlignment*>{&different_node}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&different_orientation}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&different_offset}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&different_from}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&different_to}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&extra_empty_subpath}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&extra_zero_base_mapping}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&nonempty_edit_sequence}),
            "assignment signatures must retain position, edit, subpath, and zero-base mapping structure");
    require(assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&nonempty_edit_sequence}) ==
                assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&different_nonempty_edit_sequence}),
            "assignment signatures must ignore nonempty edit sequence bases");
    const auto next_graph = split_read(1, false, 2, false);
    auto without_next = next_graph;
    without_next.mutable_subpath(0)->clear_next();
    const auto connection_graph = scored_connection_read();
    auto without_connection = connection_graph;
    without_connection.mutable_subpath(0)->clear_connection();
    auto ignored_connection_score = connection_graph;
    ignored_connection_score.mutable_subpath(0)->mutable_connection(0)->set_score(99);
    require(assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&next_graph}) !=
                assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&without_next}) &&
                assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&connection_graph}) !=
                    assignment_graph_signature(
                        std::vector<const vg::MultipathAlignment*>{&without_connection}),
            "assignment signatures must retain next and connection topology");
    require(graph_signature == assignment_graph_signature(
                std::vector<const vg::MultipathAlignment*>{&ignored_subpath_score}) &&
                assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&connection_graph}) ==
                    assignment_graph_signature(
                        std::vector<const vg::MultipathAlignment*>{&ignored_connection_score}),
            "assignment signatures must ignore subpath and connection scores");
    auto ignored_path_metadata = graph;
    auto* ignored_mapping = ignored_path_metadata.mutable_subpath(0)->mutable_path()->mutable_mapping(0);
    ignored_path_metadata.mutable_subpath(0)->mutable_path()->set_name("ignored-path");
    ignored_path_metadata.mutable_subpath(0)->mutable_path()->set_is_circular(true);
    ignored_path_metadata.mutable_subpath(0)->mutable_path()->set_length(99);
    ignored_mapping->set_rank(7);
    ignored_mapping->mutable_position()->set_name("ignored-position");
    require(graph_signature == assignment_graph_signature(
                std::vector<const vg::MultipathAlignment*>{&ignored_path_metadata}),
            "assignment signatures must ignore mapping rank and path metadata");
    vg::MultipathAlignment absent_position;
    absent_position.add_subpath()->mutable_path()->add_mapping()->add_edit();
    auto default_position = absent_position;
    default_position.mutable_subpath(0)->mutable_path()->mutable_mapping(0)->mutable_position();
    require(assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&absent_position}) ==
                assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&default_position}),
            "assignment signatures must preserve the scorer's absent-position accessor semantics");
    const auto second_graph = read({{2, false}});
    vg::MultipathAlignment empty_record;
    require(assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&graph, &second_graph}) !=
                assignment_graph_signature(std::vector<const vg::MultipathAlignment*>{&second_graph, &graph}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&graph, &graph}) &&
                empty_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&empty_record}) &&
                graph_signature != assignment_graph_signature(
                    std::vector<const vg::MultipathAlignment*>{&graph, &empty_record}),
            "assignment signatures must retain empty groups, record order, and boundaries");
    threw = false;
    try {
        static_cast<void>(assignment_graph_signature(
            std::vector<const vg::MultipathAlignment*>{nullptr}));
    } catch (const std::runtime_error& error) {
        threw = error.what() == std::string("prepared count received null alignment");
    }
    require(threw, "assignment signature must preserve the resolver null-alignment failure");

    SupportSet support;
    require(support.empty(), "support storage must preserve empty-set behavior");
    support.insert(7);
    require(support.size() == 1 && *support.begin() == 7,
            "support storage must preserve a singleton inline value");
    support.insert(3);
    support.insert(7);
    require(std::vector<std::uint32_t>(support.begin(), support.end()) ==
                std::vector<std::uint32_t>{3, 7},
            "support storage must preserve sorted unique order");
    SupportSet copied = support;
    copied.insert(11);
    require(std::vector<std::uint32_t>(support.begin(), support.end()) ==
                std::vector<std::uint32_t>{3, 7} &&
                std::vector<std::uint32_t>(copied.begin(), copied.end()) ==
                    std::vector<std::uint32_t>{3, 7, 11},
            "support storage copies must not alias their inline contents");
    SupportSet moved = std::move(copied);
    require(std::vector<std::uint32_t>(moved.begin(), moved.end()) ==
                std::vector<std::uint32_t>{3, 7, 11},
            "support storage moves must preserve ordered values");
    SupportSet lexicographically_later;
    lexicographically_later.insert(3);
    lexicographically_later.insert(9);
    require(support < lexicographically_later,
            "support storage must retain std::set lexical comparison semantics");
    moved.clear();
    require(moved.empty(), "support storage clear must release all logical values");
    SupportIds ids;
    for (std::uint32_t value = 9; value != 0; --value) ids.push_back(value);
    SupportSet spilled;
    spilled.insert(ids.begin(), ids.end());
    require(spilled.size() == 9 && *spilled.begin() == 1 && *spilled.rbegin() == 9,
            "support storage must preserve ordering after exceeding inline capacity");
}

}  // namespace

int main() try {
    require_cache_signature_and_support_storage();
    TemporaryDirectory temporary;
    const auto xg_path = temporary.path() / "graph.xg";
    const auto gbwt_path = temporary.path() / "paths.gbwt";
    const auto r_index_path = temporary.path() / "paths.ri";
    const auto index_path = temporary.path() / "native.pci";

    // Rows 0/2 are distinct raw Parents with identical exon walks. Rows 6/8
    // establish a known splice by distributing its body endpoints across raw
    // body fragments; row 4 is a negative-genomic-strand transcript path.
    const std::vector<std::vector<std::pair<std::uint64_t, bool>>> walks = {
        {{1, false}, {2, false}}, {{3, false}, {4, false}},
        {{1, false}, {2, false}}, {{5, false}, {6, false}},
        {{7, false}, {8, false}}, {{9, true}, {10, true}},
        {{11, false}, {12, false}}, {{11, false}},
        {{11, false}, {12, false}}, {{12, false}},
        {{13, false}, {14, false}}, {{13, false}},
        {{15, false}, {16, false}, {15, false}, {15, true}}, {{15, false}, {17, false}, {18, false}},
        {{20, false}, {21, false}}, {{21, true}, {20, true}},
        {{15, false}},
    };
    write_xg(xg_path);
    const gbwt::GBWT gbwt = write_gbwt(gbwt_path, walks);
    write_r_index(r_index_path, gbwt);
    require(gbwt.hasMetadata() && gbwt.metadata.paths() == walks.size(),
            "GBWT fixture must retain path metadata before native loading");
    gbwt::FastLocate before_clear;
    {
        std::ifstream input(r_index_path, std::ios::binary);
        require(static_cast<bool>(input), "cannot reopen native-query r-index fixture");
        before_clear.load(input);
    }
    before_clear.setGBWT(gbwt);
    const auto before_clear_snapshot = snapshot_gbwt(gbwt, before_clear);
    auto metadata_cleared = gbwt;
    metadata_cleared.clearMetadata();
    gbwt::FastLocate after_clear;
    {
        std::ifstream input(r_index_path, std::ios::binary);
        require(static_cast<bool>(input), "cannot reopen cleared native-query r-index fixture");
        after_clear.load(input);
    }
    after_clear.setGBWT(metadata_cleared);
    require(!metadata_cleared.hasMetadata() && metadata_cleared.metadata.paths() == 0 &&
                metadata_cleared.metadata.samples() == 0 && metadata_cleared.metadata.contigs() == 0,
            "cleared GBWT must release all metadata");
    require(before_clear_snapshot == snapshot_gbwt(metadata_cleared, after_clear),
            "GBWT metadata clearing must preserve FastLocate path queries");

    Data data;
    data.strings = {"TX_SHARED", "TX_MINUS", "TX_SPLICE", "TX_UNKNOWN", "TX_REPEAT",
                    "TX_BLOCKED", "GENE", "test_vetoed_category"};
    data.feature_catalog = {"GENE"};
    data.sets = {{}, {101}, {202}, {303}, {7}};
    data.genes.resize(1);
    data.genes[0].id_string = 6;
    data.genes[0].equivalence[0] = 0;
    data.genes[0].competition[0] = 0;
    data.policies.resize(4);
    for (auto& policy : data.policies) policy.gene = 0;
    data.policies[0].category_set = 4;
    data.policies[2].category_set = 4;
    data.paths = {
        path_record(0, 0, 0, pancollapse::native_count::exon_path, 0),
        path_record(0, 1, 1, pancollapse::native_count::body_path, 0),
        path_record(0, 2, 0, pancollapse::native_count::exon_path, 0),
        path_record(0, 3, 2, pancollapse::native_count::body_path, 0),
        path_record(1, 0, 0, pancollapse::native_count::exon_path, 1),
        path_record(1, 1, 3, pancollapse::native_count::body_path, 1),
        path_record(2, 0, 0, pancollapse::native_count::exon_path, 1),
        path_record(2, 1, 1, pancollapse::native_count::body_path, 1),
        path_record(2, 2, 0, pancollapse::native_count::exon_path, 1),
        path_record(2, 3, 2, pancollapse::native_count::body_path, 1),
        path_record(3, 0, 0, pancollapse::native_count::exon_path, 0),
        path_record(3, 1, 3, pancollapse::native_count::body_path, 0),
        path_record(4, 0, 0, pancollapse::native_count::exon_path, 0),
        path_record(4, 1, 1, pancollapse::native_count::body_path, 0),
        path_record(5, 0, 0, pancollapse::native_count::exon_path, 0),
        path_record(5, 1, 3, pancollapse::native_count::body_path, 0),
    };
    data.pairs = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}, {10, 11}, {12, 13}, {14, 15}};
    for (std::uint32_t model = 0; model < data.pairs.size(); ++model) {
        for (const auto row : {data.pairs[model].exon, data.pairs[model].body}) {
            data.paths[row].model_first = data.model_refs.size();
            data.paths[row].model_count = 1;
            data.model_refs.push_back(model);
        }
    }
    data.path_lookup.resize(walks.size());
    std::iota(data.path_lookup.begin(), data.path_lookup.end(), 0U);
    data.path_lookup.back() = pancollapse::count_reference::missing;
    data.gbwt_size_bytes = std::filesystem::file_size(gbwt_path);
    data.content_id = "native-query-test";
    pancollapse::native_count::write(index_path, data);

    auto source = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 4);
    const auto at_load = source->stats();
    require(at_load.native_queries == 0 && at_load.native_occurrences == 0 &&
                at_load.node_queries == 0 && at_load.edge_queries == 0 &&
                at_load.edge_model_builds == 0 &&
                at_load.max_node_models == 0,
            "native load must not pre-query paths or graph evidence");
    require(source->take_feature_catalog() == std::vector<std::string>{"GENE"},
            "native feature catalog must be load-ready metadata");
    require(source->sets().size() == 5 && source->sets()[1] == std::vector<std::uint32_t>{101} &&
                source->sets()[2] == std::vector<std::uint32_t>{202} &&
                source->sets()[4] == std::vector<std::uint32_t>{7},
            "native support sets must survive metadata load");

    require(source->model_count() == 8, "raw Parent pairs must not be geometry-merged");
    const auto shared_a = source->model(0);
    const auto shared_b = source->model(1);
    require(shared_a.transcript_string == 0 && shared_a.exon_policy == 0 &&
                shared_a.body_policy == 1,
            "first raw Parent model must retain own exon/body policies");
    require(shared_b.transcript_string == 0 && shared_b.exon_policy == 2 &&
                shared_b.body_policy == 3,
            "second raw Parent model must retain own exon/body policies");

    const auto* first_node = source->node(1);
    require(first_node != nullptr, "native node evidence missing");
    const auto first_postings = source->node_postings(*first_node);
    require(first_postings.size() == 2 && first_postings[0].model == 0 &&
                first_postings[1].model == 1 &&
                first_postings[0].flags == kExonForward && first_postings[1].flags == kExonForward,
            "identical exon geometry must retain separate raw Parent postings");
    const auto first_scoring = source->node_scoring(*first_node);
    require(first_scoring.size() == 2 && first_scoring[0].class_id == 0 &&
                first_scoring[1].class_id == 2,
            "raw alias walks must remain independent scoring classes");
    require(posting_for(source->node_postings(*source->node(3)), 0).support_set == 1 &&
                posting_for(source->node_postings(*source->node(5)), 1).support_set == 2,
            "body paths must retain their own raw Parent support sets");

    const auto minus = source->model(2);
    require(minus.strand == 1, "minus transcript strand must be retained in its model");
    const auto ordinary = source->edge(oriented(9, true), oriented(10, true));
    require((edge_for(ordinary, 2).flags & kOrdinaryBodyEdge) != 0,
            "reverse-orientation body edge must remain ordinary body evidence");
    require(source->edge(oriented(9, false), oriented(10, false)).empty(),
            "opposite physical orientation must not manufacture an ordinary edge");

    const auto splice_before = source->stats();
    const auto known_targets = source->splice_targets(11, 12);
    require(known_targets.size() == 1 && known_targets[0] == 2,
            "separate body fragments with exonic adjacency must form the known splice target");
    const auto* known_targets_data = known_targets.data();
    const auto reverse_known_targets = source->splice_targets(12, 11);
    require(reverse_known_targets.data() == known_targets_data && reverse_known_targets.size() == 1 &&
                reverse_known_targets[0] == 2 &&
                source->stats().splice_cache.builds == splice_before.splice_cache.builds + 1,
            "physical splice keys must be canonical across endpoint order");
    require(source->splice_targets(11, 11).empty() &&
                source->splice_targets(static_cast<std::uint64_t>(-1),
                                       static_cast<std::uint64_t>(-1)).empty(),
            "same-node physical splice queries, including a signed sentinel, must be empty");
    const auto splice_after_same = source->stats();
    require(splice_after_same.splice_cache.builds == splice_before.splice_cache.builds + 3,
            "same-node splice results must be immutable cached evidence");
    const auto malformed_splice = [&] {
        try {
            static_cast<void>(source->splice_targets(static_cast<std::uint64_t>(-1), 1));
        } catch (const std::runtime_error& error) {
            return std::string(error.what());
        }
        throw std::runtime_error("invalid physical splice endpoint was accepted");
    };
    const std::string malformed_splice_error = malformed_splice();
    require(malformed_splice() == malformed_splice_error &&
                source->stats().splice_cache.builds == splice_after_same.splice_cache.builds &&
                source->stats().splice_cache.entries == splice_after_same.splice_cache.entries,
            "failed splice construction must not publish partial shared evidence");
    require(source->splice_targets(13, 14).empty(),
            "missing one body endpoint must leave a splice target unknown");
    const auto along = source->junction_owners(oriented(11, false), oriented(12, false));
    require(along.size() == 1 && owner_for(along, 2).flags == kAlongTranscript,
            "minus-gene forward RNA path must remain along-transcript");
    const auto reverse = source->junction_owners(oriented(12, true), oriented(11, true));
    require(reverse.size() == 1 && owner_for(reverse, 2).flags == kReverseComplement,
            "minus-gene reverse-complement path must remain reverse-complement");
    const std::array wrong_orientations = {
        std::pair{oriented(11, false), oriented(12, true)},
        std::pair{oriented(11, true), oriented(12, false)},
        std::pair{oriented(11, true), oriented(12, true)},
    };
    for (const auto& [from, to] : wrong_orientations) {
        require(source->junction_owners(from, to).empty(),
                "nonexistent splice orientation must not gain a junction owner");
    }
    require(source->junction_owners(oriented(13, false), oriented(14, false)).empty(),
            "unknown splice must not gain a junction owner");
    const auto blocked_body = source->edge(oriented(21, true), oriented(20, true));
    require((edge_for(blocked_body, 7).flags & kOrdinaryBodyEdge) != 0 &&
                source->splice_targets(20, 21).empty(),
            "body adjacency in any orientation must veto a physical splice target");

    const auto forward_profile = pancollapse::compile_numeric_profile(*source,
        pancollapse::direct_count::effective_profile(pancollapse::direct_count::ProfileId::cr7_v1));
    const auto forward = read({{11, false}, {12, false}});
    pancollapse::NodeScoreResolver resolver(*source);
    const auto expected = resolver.resolve({&forward}, {&forward_profile, 1});
    std::array<std::future<bool>, 4> concurrent;
    for (auto& future : concurrent) {
        future = std::async(std::launch::async, [&] {
            const auto actual = resolver.resolve({&forward}, {&forward_profile, 1});
            return actual.size() == expected.size() &&
                actual.front().facts.exact == expected.front().facts.exact &&
                actual.front().facts.gene_fallback == expected.front().facts.gene_fallback &&
                actual.front().result.genes == expected.front().result.genes &&
                actual.front().result.terminal == expected.front().result.terminal &&
                actual.front().result.reasons == expected.front().result.reasons;
        });
    }
    for (auto& future : concurrent)
        require(future.get(), "shared native node-score resolution must be deterministic");

    const auto* repeated_node = source->node(15);
    const auto repeated_scoring = source->node_scoring(*repeated_node);
    const auto& repeated = scoring_for(repeated_scoring, 12);
    require(repeated.forward == 2 && repeated.reverse == 1,
            "repeated raw visits must preserve scoring multiplicity");
    require(source->node_postings(*repeated_node).size() == 1 &&
                source->node_postings(*repeated_node)[0].model == 6,
            "repeated visits must not create a duplicate raw model");

    static_cast<void>(source->node(1));
    static_cast<void>(source->edge(oriented(1, false), oriented(2, false)));
    const auto before_reset = source->stats();
    source->begin_read_group();
    require(source->stats().active_lease_bytes == 0,
            "group reset must release all worker evidence leases");
    const auto* reused_node = source->node(1);
    const auto reused_postings = source->node_postings(*reused_node);
    const auto reused_edge = source->edge(oriented(1, false), oriented(2, false));
    const auto after_reset = source->stats();
    require(after_reset.native_queries == before_reset.native_queries &&
                after_reset.node_cache.builds == before_reset.node_cache.builds &&
                after_reset.edge_cache.builds == before_reset.edge_cache.builds &&
                after_reset.node_cache.hits > before_reset.node_cache.hits &&
                after_reset.edge_cache.hits > before_reset.edge_cache.hits,
            "group reset must reuse immutable node/edge evidence without native queries");
    require(after_reset.active_lease_bytes > 0 &&
                after_reset.active_lease_high_water_bytes >= after_reset.active_lease_bytes,
            "shared evidence leases must be conservatively accounted");
    const auto reused_flags = posting_for(reused_postings, 0).flags;
    const auto reused_edge_flags = edge_for(reused_edge, 0).flags;
    static_cast<void>(source->junction_owners(oriented(11, false), oriented(12, false)));
    require(reused_node->id == 1 && posting_for(reused_postings, 0).flags == reused_flags &&
                edge_for(reused_edge, 0).flags == reused_edge_flags,
            "derived lookups must not invalidate active node records or posting spans");
    require(after_reset.max_node_occurrences >= 2 && after_reset.max_node_models >= 2,
            "native query stats must retain observed fanout without capping it");
    source->begin_read_group();
    require(source->stats().active_lease_bytes == 0,
            "repeated group reset must return current lease bytes to zero");

    auto node_score_source = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score);
    const auto same_node_record = [](const auto& left, const auto& right) {
        return left.id == right.id && left.length == right.length && left.first == right.first &&
            left.count == right.count && left.reserved == right.reserved &&
            left.score_first == right.score_first && left.score_count == right.score_count &&
            left.reserved2 == right.reserved2;
    };
    const auto same_postings = [](auto left, auto right) {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
            [](const Posting& a, const Posting& b) {
                return a.model == b.model && a.support_set == b.support_set && a.flags == b.flags;
            });
    };
    const auto same_scoring = [](auto left, auto right) {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
            [](const ScoringPosting& a, const ScoringPosting& b) {
                return a.class_id == b.class_id && a.forward == b.forward && a.reverse == b.reverse;
            });
    };
    auto distinct_full = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1);
    auto distinct_slim = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score);
    const auto* distinct_full_node = distinct_full->node(15);
    const auto* distinct_slim_node = distinct_slim->node(15);
    const auto distinct_full_stats = distinct_full->stats();
    const auto distinct_slim_stats = distinct_slim->stats();
    const auto distinct_postings = distinct_slim->node_postings(*distinct_slim_node);
    require(same_postings(distinct_full->node_postings(*distinct_full_node), distinct_postings) &&
                distinct_slim_node->score_count == 2 && distinct_postings.size() == 1 &&
                distinct_postings[0].model == 6 &&
                distinct_postings[0].flags == (kExonForward | kExonReverse | kBodyForward) &&
                distinct_postings[0].support_set == 1 &&
                distinct_slim_stats.native_occurrences == distinct_full_stats.native_occurrences &&
                distinct_slim_stats.max_node_occurrences == distinct_full_stats.max_node_occurrences,
            "node-score construction must reduce distinct annotated rows while retaining raw occurrence accounting");
    const auto before_distinct_lazy = distinct_slim->stats();
    const auto distinct_lazy_scoring = distinct_slim->node_scoring(*distinct_slim_node);
    require(same_scoring(distinct_lazy_scoring, distinct_full->node_scoring(*distinct_full_node)) &&
                distinct_lazy_scoring.size() == 2 &&
                scoring_for(distinct_lazy_scoring, 12).forward == 2 &&
                scoring_for(distinct_lazy_scoring, 12).reverse == 1 &&
                scoring_for(distinct_lazy_scoring, 13).forward == 1 &&
                distinct_slim->stats().native_queries == before_distinct_lazy.native_queries + 1,
            "lazy node scoring must retain exact raw visits after distinct node-score construction");
    distinct_slim->begin_read_group();
    const auto before_distinct_warm = distinct_slim->stats();
    static_cast<void>(distinct_slim->node_postings(*distinct_slim->node(15)));
    require(distinct_slim->stats().native_queries == before_distinct_warm.native_queries &&
                distinct_slim->stats().node_cache.builds == before_distinct_warm.node_cache.builds,
            "warm distinct node-score evidence must not query again after a worker reset");
    auto full_charge_source = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1);
    auto slim_charge_source = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score);
    for (const std::uint64_t id : {1ULL, 3ULL, 9ULL, 15ULL}) {
        const auto* full_node = full_charge_source->node(id);
        const auto* slim_node = slim_charge_source->node(id);
        require(same_node_record(*full_node, *slim_node) &&
                    same_postings(full_charge_source->node_postings(*full_node),
                                  slim_charge_source->node_postings(*slim_node)) &&
                    same_scoring(full_charge_source->node_scoring(*full_node),
                                 slim_charge_source->node_scoring(*slim_node)),
                "slim node evidence must preserve full-mode metadata, postings, and scoring");
    }
    require(slim_charge_source->stats().node_cache.resident_bytes <
                full_charge_source->stats().node_cache.resident_bytes,
            "slim node-score evidence must charge fewer cached bytes for equal queried nodes");
    auto fanout_data = data;
    for (std::size_t i = 0; i < 4096; ++i) fanout_data.pairs.push_back({0, 1});
    fanout_data.model_refs.clear();
    for (std::uint32_t row = 0; row < fanout_data.paths.size(); ++row) {
        auto& path = fanout_data.paths[row];
        path.model_first = fanout_data.model_refs.size();
        path.model_count = 0;
        for (std::uint32_t model = 0; model < fanout_data.pairs.size(); ++model) {
            const auto& pair = fanout_data.pairs[model];
            if (pair.exon == row || pair.body == row) {
                fanout_data.model_refs.push_back(model);
                ++path.model_count;
            }
        }
    }
    const auto fanout_index_path = temporary.path() / "native-compressed-fanout.pci";
    pancollapse::native_count::write(fanout_index_path, fanout_data);
    auto fanout_full = NativeSource::load(fanout_index_path, xg_path, gbwt_path, r_index_path, 1);
    auto fanout_slim = NativeSource::load(fanout_index_path, xg_path, gbwt_path, r_index_path, 1,
                                        pancollapse::NativeQueryMode::node_score);
    const auto* full_fanout_node = fanout_full->node(1);
    const auto full_fanout_postings = fanout_full->node_postings(*full_fanout_node);
    const auto* slim_fanout_node = fanout_slim->node(1);
    const auto encoded_fanout_stats = fanout_slim->stats();
    const auto slim_fanout_postings = fanout_slim->node_postings(*slim_fanout_node);
    require(full_fanout_postings.size() > 4096 &&
                same_node_record(*full_fanout_node, *slim_fanout_node) &&
                same_postings(full_fanout_postings, slim_fanout_postings) &&
                encoded_fanout_stats.node_cache.resident_bytes * 2 <
                    fanout_full->stats().node_cache.resident_bytes,
            "compressed native cache must preserve every raw model while reducing retained bytes");
    const auto decoded_fanout_stats = fanout_slim->stats();
    const auto slim_fanout_again = fanout_slim->node_postings(*slim_fanout_node);
    require(slim_fanout_again.data() == slim_fanout_postings.data() &&
                decoded_fanout_stats.native_queries == encoded_fanout_stats.native_queries &&
                fanout_slim->stats().active_lease_bytes == decoded_fanout_stats.active_lease_bytes,
            "decoded node postings must be reused once per worker/group without querying or recharging");
    static_cast<void>(fanout_slim->node_postings(*fanout_slim->node(3)));
    require(same_postings(slim_fanout_postings, full_fanout_postings),
            "decoded node posting spans must survive other node insertions");
    fanout_slim->begin_read_group();
    require(fanout_slim->stats().active_lease_bytes == 0,
            "group reset must release compressed node leases and decoded posting charges");
    const auto before_redecode = fanout_slim->stats();
    require(same_postings(fanout_slim->node_postings(*fanout_slim->node(1)), full_fanout_postings) &&
                fanout_slim->stats().native_queries == before_redecode.native_queries &&
                fanout_slim->stats().node_cache.builds == before_redecode.node_cache.builds,
            "reset must decode retained compressed postings without a new native lookup");
    fanout_slim->begin_read_group();
    std::array<std::future<bool>, 4> compressed_node_clients;
    for (auto& future : compressed_node_clients) {
        future = std::async(std::launch::async, [&] {
            const auto* node = fanout_slim->node(1);
            const auto first = fanout_slim->node_postings(*node);
            const auto second = fanout_slim->node_postings(*node);
            return first.data() == second.data() && same_postings(first, full_fanout_postings);
        });
    }
    for (auto& future : compressed_node_clients)
        require(future.get(), "concurrent compressed node clients must receive exact independent decoded spans");
    require(fanout_slim->stats().active_lease_bytes == 0 &&
                fanout_slim->stats().native_queries == before_redecode.native_queries,
            "finished compressed-node workers must release vectors without requerying the shared cache");
    const auto* raw_node = fanout_slim->node(15);
    const auto before_raw_postings = fanout_slim->stats();
    const auto raw_postings = fanout_slim->node_postings(*raw_node);
    require(raw_postings.size() == 1 &&
                same_postings(raw_postings, fanout_full->node_postings(*fanout_full->node(15))) &&
                raw_postings.data() == fanout_slim->node_postings(*raw_node).data() &&
                fanout_slim->stats().active_lease_bytes == before_raw_postings.active_lease_bytes,
            "uncompressed native fallback must directly reuse typed postings without a decoded allocation");
    const auto* empty_node = fanout_slim->node(19);
    const auto before_empty_postings = fanout_slim->stats();
    require(empty_node->count == 0 && fanout_slim->node_postings(*empty_node).empty() &&
                same_node_record(*empty_node, *fanout_full->node(19)) &&
                fanout_slim->stats().active_lease_bytes == before_empty_postings.active_lease_bytes,
            "empty native nodes must retain empty raw postings without a decoded allocation");
    fanout_slim->begin_read_group();
    for (const std::uint64_t id : {1ULL, 3ULL, 9ULL, 15ULL}) {
        const auto* full_node = source->node(id);
        const auto* slim_node = node_score_source->node(id);
        require(same_node_record(*full_node, *slim_node) &&
                    same_postings(source->node_postings(*full_node),
                                  node_score_source->node_postings(*slim_node)),
                "node-score slim evidence must retain exact node metadata and postings before scoring getter");
    }
    const auto* slim_repeated_node = node_score_source->node(15);
    const auto scoring_before_getter = node_score_source->stats();
    const auto slim_repeated_scoring = node_score_source->node_scoring(*slim_repeated_node);
    const auto scoring_after_first_getter = node_score_source->stats();
    const auto slim_repeated_scoring_again = node_score_source->node_scoring(*slim_repeated_node);
    const auto scoring_after_second_getter = node_score_source->stats();
    require(same_scoring(slim_repeated_scoring, source->node_scoring(*source->node(15))) &&
                scoring_after_first_getter.native_queries == scoring_before_getter.native_queries + 1 &&
                scoring_after_second_getter.native_queries == scoring_after_first_getter.native_queries &&
                slim_repeated_scoring.data() == slim_repeated_scoring_again.data(),
            "node-score scoring getter must reconstruct exact repeated multiplicities once per worker group");
    const auto* retained_scoring_data = slim_repeated_scoring.data();
    static_cast<void>(node_score_source->node_scoring(*node_score_source->node(1)));
    require(slim_repeated_scoring.data() == retained_scoring_data &&
                same_scoring(slim_repeated_scoring, source->node_scoring(*source->node(15))),
            "worker-local lazy scoring spans must survive other node insertions");
    for (const std::uint64_t id : {1ULL, 3ULL, 9ULL}) {
        require(same_scoring(node_score_source->node_scoring(*node_score_source->node(id)),
                             source->node_scoring(*source->node(id))),
                "lazy scoring must preserve full-mode class order and both orientation counts");
    }
    node_score_source->begin_read_group();
    require(node_score_source->stats().active_lease_bytes == 0,
            "group reset must release lazy node scoring charges");
    const auto scoring_before_reset_getter = node_score_source->stats();
    const auto slim_reconstructed_scoring = node_score_source->node_scoring(*node_score_source->node(15));
    require(same_scoring(slim_reconstructed_scoring, source->node_scoring(*source->node(15))) &&
                node_score_source->stats().native_queries == scoring_before_reset_getter.native_queries + 1 &&
                node_score_source->stats().node_cache.builds == scoring_before_reset_getter.node_cache.builds,
            "group reset must release and lazily reconstruct exact node scoring evidence");
    pancollapse::NodeScoreResolver node_score_resolver(*node_score_source);
    static_cast<void>(node_score_resolver.resolve({&forward}, {&forward_profile, 1}));
    require(node_score_source->stats().edge_model_builds == 0,
            "pure node-score resolution must not materialize edge model postings");
    const auto node_score_edge = node_score_source->edge(oriented(1, false), oriented(2, false));
    require(edge_for(node_score_edge, 0).flags == reused_edge_flags &&
                node_score_source->stats().edge_model_builds > 0,
            "node-score source edge() must exactly materialize legacy postings on demand");
    const auto* retained_node_score_edge_data = node_score_edge.data();
    const auto retained_node_score_edge_flags = edge_for(node_score_edge, 0).flags;
    static_cast<void>(node_score_source->edge(oriented(9, true), oriented(10, true)));
    const auto slim_splice = node_score_source->splice_targets(11, 12);
    const auto slim_owner = node_score_source->junction_owners(oriented(11, false), oriented(12, false));
    require(node_score_edge.data() == retained_node_score_edge_data &&
                edge_for(node_score_edge, 0).flags == retained_node_score_edge_flags &&
                slim_splice.size() == 1 && slim_splice[0] == 2 &&
                slim_owner.size() == 1 && slim_owner[0].transcript_string == 2 &&
                slim_owner[0].flags == kAlongTranscript &&
                node_score_source->splice_targets(20, 21).empty(),
            "node-score worker-local edge postings must retain span lifetime within a read group");
    auto probe_full = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1);
    auto probe_enabled = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score, 4096,
        false);
    auto probe_disabled = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1,
        pancollapse::NativeQueryMode::node_score, 0, false);
    auto probe_budget = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1,
        pancollapse::NativeQueryMode::node_score, 1, false);
    auto probe_common_empty = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score, 4096,
        false);
    const auto same_targets = [](auto left, auto right) {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
    };
    for (const auto& pair : std::array<std::pair<std::uint64_t, std::uint64_t>, 5>{
             {{9, 10}, {11, 12}, {12, 11}, {13, 14}, {20, 21}}}) {
        const auto full_targets = probe_full->splice_targets(pair.first, pair.second);
        require(same_targets(probe_enabled->splice_targets(pair.first, pair.second), full_targets) &&
                    same_targets(probe_disabled->splice_targets(pair.first, pair.second), full_targets) &&
                    same_targets(probe_budget->splice_targets(pair.first, pair.second), full_targets),
                "enabled, disabled, and budgeted splice probes must retain exact full-mode targets");
    }
    require(probe_enabled->splice_targets(15, 15).empty() &&
                probe_enabled->stats().splice_probe_success > 0 &&
                probe_enabled->stats().splice_probe_exhausted_without_proof > 0 &&
                probe_disabled->stats().splice_probe_budget_fallback > 0 &&
                probe_budget->stats().splice_probe_budget_fallback > 0,
            "native splice probe seams must cover proof, exhaustion, disabled, and forced-budget fallback");
    const auto common_empty_before = probe_common_empty->stats();
    require(probe_common_empty->splice_targets(1, 2).empty() &&
                probe_common_empty->stats().splice_probe_common_empty ==
                    common_empty_before.splice_probe_common_empty + 1 &&
                probe_common_empty->stats().edge_queries == common_empty_before.edge_queries,
            "empty common body targets must avoid oriented edge evidence queries");
    const auto full_probe_owner = probe_full->junction_owners(oriented(11, false), oriented(12, false));
    const auto enabled_probe_owner = probe_enabled->junction_owners(oriented(11, false), oriented(12, false));
    require(full_probe_owner.size() == enabled_probe_owner.size() &&
                std::equal(full_probe_owner.begin(), full_probe_owner.end(), enabled_probe_owner.begin(),
                    [](const JunctionOwner& left, const JunctionOwner& right) {
                        return left.transcript_string == right.transcript_string && left.flags == right.flags;
                    }),
            "splice probe must retain exact junction owners for the true junction");
    bool invalid_probe_endpoint = false;
    try {
        static_cast<void>(probe_enabled->splice_targets(static_cast<std::uint64_t>(-1), 1));
    } catch (const std::runtime_error& error) {
        invalid_probe_endpoint = std::string(error.what()).find("GAMP node is absent") != std::string::npos;
    }
    require(invalid_probe_endpoint,
            "probe splice path must retain invalid distinct-endpoint validation");

    auto certificate_full = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1);
    auto certificate_default = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score);
    auto certificate_disabled = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1,
        pancollapse::NativeQueryMode::node_score, 4096, false);
    for (std::uint64_t from = 1; from <= 21; ++from) {
        for (std::uint64_t to = 1; to <= 21; ++to) {
            if (from == to) continue;
            const auto full_targets = certificate_full->splice_targets(from, to);
            const auto default_targets = certificate_default->splice_targets(from, to);
            const auto disabled_targets = certificate_disabled->splice_targets(from, to);
            require(same_targets(default_targets, full_targets) &&
                        same_targets(disabled_targets, full_targets),
                    "adjacency certificate must preserve full-mode splice targets for every physical pair");
        }
    }
    const auto certificate_default_stats = certificate_default->stats();
    const auto certificate_disabled_stats = certificate_disabled->stats();
    require(certificate_default_stats.splice_adjacency_checks > 0 &&
                certificate_default_stats.splice_adjacency_success > 0 &&
                certificate_default_stats.edge_queries <= certificate_disabled_stats.edge_queries &&
                certificate_default_stats.splice_probe_sa_steps <
                    certificate_disabled_stats.splice_probe_sa_steps,
            "adjacency certificate must avoid SA work without increasing edge queries");
    // Small ordinary pairs already succeed in the bounded SA witness probe.
    // Put unannotated paths ahead of the body witness to exercise the real
    // budget-exhaustion case: the certificate must avoid full edge decoding.
    constexpr std::size_t decoys = 512;
    std::vector<std::vector<std::pair<std::uint64_t, bool>>> heavy_walks(
        decoys, {{9, true}, {10, true}});
    heavy_walks.insert(heavy_walks.end(), walks.begin(), walks.end());
    const auto heavy_gbwt_path = temporary.path() / "adjacency-heavy.gbwt";
    const auto heavy_ri_path = temporary.path() / "adjacency-heavy.ri";
    const auto heavy_index_path = temporary.path() / "adjacency-heavy.pci";
    const auto heavy_gbwt = write_gbwt(heavy_gbwt_path, heavy_walks);
    write_r_index(heavy_ri_path, heavy_gbwt);
    Data heavy_data = data;
    heavy_data.path_lookup.assign(heavy_walks.size(), pancollapse::count_reference::missing);
    std::copy(data.path_lookup.begin(), data.path_lookup.end(), heavy_data.path_lookup.begin() + decoys);
    heavy_data.gbwt_size_bytes = std::filesystem::file_size(heavy_gbwt_path);
    heavy_data.content_id = "native-query-heavy-adjacency";
    pancollapse::native_count::write(heavy_index_path, heavy_data);
    auto heavy_full = NativeSource::load(heavy_index_path, xg_path, heavy_gbwt_path, heavy_ri_path, 1);
    auto heavy_default = NativeSource::load(heavy_index_path, xg_path, heavy_gbwt_path, heavy_ri_path, 1,
        pancollapse::NativeQueryMode::node_score);
    auto heavy_disabled = NativeSource::load(heavy_index_path, xg_path, heavy_gbwt_path, heavy_ri_path, 1,
        pancollapse::NativeQueryMode::node_score, 4096, false);
    const auto heavy_targets = heavy_full->splice_targets(9, 10);
    require(heavy_targets.empty() && same_targets(heavy_default->splice_targets(9, 10), heavy_targets) &&
                same_targets(heavy_disabled->splice_targets(9, 10), heavy_targets),
            "missing annotation rows must preserve exact universal-adjacency targets");
    require(heavy_default->stats().splice_adjacency_success == 1 &&
                heavy_default->stats().edge_queries == 0 &&
                heavy_default->stats().splice_probe_sa_steps == 0 &&
                heavy_disabled->stats().splice_probe_budget_fallback > 0 &&
                heavy_disabled->stats().edge_queries > 0,
            "universal adjacency must avoid budget-exhausted raw edge decoding");
    const auto invalid_node = std::numeric_limits<std::uint64_t>::max();
    require(certificate_default->splice_targets(invalid_node, invalid_node).empty(),
            "same-node early return must remain unchanged even for a sentinel node");
    for (const auto& [first, second] : std::array<std::pair<std::uint64_t, std::uint64_t>, 4>{
             {{0, 9}, {9, 0}, {invalid_node, 9}, {9, invalid_node}}}) {
        bool rejected = false;
        try { (void)certificate_default->splice_targets(first, second); }
        catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).find("GAMP node is absent") != std::string::npos;
        }
        require(rejected, "adjacency certificate must preserve both distinct XG endpoint validations");
    }
    const auto certificate_full_owners =
        certificate_full->junction_owners(oriented(11, false), oriented(12, false));
    const auto certificate_default_owners =
        certificate_default->junction_owners(oriented(11, false), oriented(12, false));
    require(certificate_full_owners.size() == certificate_default_owners.size() &&
                std::equal(certificate_full_owners.begin(), certificate_full_owners.end(),
                    certificate_default_owners.begin(), [](const JunctionOwner& left, const JunctionOwner& right) {
                        return left.transcript_string == right.transcript_string && left.flags == right.flags;
                    }) &&
                certificate_default->model(2).strand == certificate_full->model(2).strand &&
                certificate_default->model(2).strand == 1,
            "adjacency certificate must retain true-junction owners and transcript strand");
    const auto node_score_before_reset = node_score_source->stats();
    node_score_source->begin_read_group();
    require(node_score_source->stats().active_lease_bytes == 0,
            "node-score edge posting materialization must be released at group reset");
    const auto node_score_reused_edge = node_score_source->edge(oriented(1, false), oriented(2, false));
    const auto node_score_after_reset = node_score_source->stats();
    require(edge_for(node_score_reused_edge, 0).flags == reused_edge_flags &&
                node_score_after_reset.edge_cache.builds == node_score_before_reset.edge_cache.builds &&
                node_score_after_reset.edge_cache.hits > node_score_before_reset.edge_cache.hits,
            "node-score edge reuse must retain immutable hit evidence across read groups");
    node_score_source->begin_read_group();

    std::array<std::future<bool>, 4> node_score_edge_clients;
    for (auto& future : node_score_edge_clients) {
        future = std::async(std::launch::async, [&] {
            const auto postings = node_score_source->edge(oriented(1, false), oriented(2, false));
            return postings.size() == 2 && edge_for(postings, 0).flags == reused_edge_flags;
        });
    }
    for (auto& future : node_score_edge_clients) {
        require(future.get(), "concurrent node-score edge clients must receive exact legacy postings");
    }
    require(node_score_source->stats().active_lease_bytes == 0,
            "finished node-score edge clients must release worker-local posting vectors");

    std::array<std::future<bool>, 4> node_score_scoring_clients;
    for (auto& future : node_score_scoring_clients) {
        future = std::async(std::launch::async, [&] {
            const auto* node = node_score_source->node(15);
            const auto first = node_score_source->node_scoring(*node);
            const auto second = node_score_source->node_scoring(*node);
            return first.data() == second.data() && first.size() == 2 &&
                first[0].class_id == 12 && first[0].forward == 2 && first[0].reverse == 1 &&
                first[1].class_id == 13 && first[1].forward == 1 && first[1].reverse == 0;
        });
    }
    for (auto& future : node_score_scoring_clients) {
        require(future.get(), "concurrent workers must reconstruct independent exact lazy scoring vectors");
    }
    require(node_score_source->stats().active_lease_bytes == 0,
            "finished node-score scoring clients must release worker-local vectors");

    auto full_edge_source = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1);
    auto slim_edge_source = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score);
    const auto same_edge_postings = [](auto left, auto right) {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
            [](const auto& a, const auto& b) { return a.model == b.model && a.flags == b.flags; });
    };
    const std::array splice_orientations = {
        std::pair{oriented(11, false), oriented(12, false)},
        std::pair{oriented(11, false), oriented(12, true)},
        std::pair{oriented(11, true), oriented(12, false)},
        std::pair{oriented(11, true), oriented(12, true)},
        std::pair{oriented(12, true), oriented(11, true)},
    };
    for (const auto& [from, to] : splice_orientations) {
        require(same_edge_postings(full_edge_source->edge(from, to), slim_edge_source->edge(from, to)),
                "node-score edge getter must preserve full-mode ordered raw model postings in every orientation");
        const auto full_owners = full_edge_source->junction_owners(from, to);
        const auto slim_owners = slim_edge_source->junction_owners(from, to);
        require(full_owners.size() == slim_owners.size() && std::equal(full_owners.begin(), full_owners.end(),
                    slim_owners.begin(), [](const JunctionOwner& a, const JunctionOwner& b) {
                        return a.transcript_string == b.transcript_string && a.flags == b.flags;
                    }), "node-score canonical edge summaries must preserve directional junction owners");
    }
    for (const auto& pair : {std::pair<std::uint64_t, std::uint64_t>{11, 12}, {12, 11}, {20, 21}}) {
        const auto full_targets = full_edge_source->splice_targets(pair.first, pair.second);
        const auto slim_targets = slim_edge_source->splice_targets(pair.first, pair.second);
        require(full_targets.size() == slim_targets.size() && std::equal(full_targets.begin(), full_targets.end(),
                    slim_targets.begin()), "node-score canonical edge summaries must preserve physical splice targets");
    }
    require(same_edge_postings(full_edge_source->edge(oriented(21, true), oriented(20, true)),
                          slim_edge_source->edge(oriented(21, true), oriented(20, true))) &&
                slim_edge_source->splice_targets(20, 21).empty(),
            "node-score canonical body summaries must retain ordinary-body splice vetoes");

    slim_edge_source->begin_read_group();
    static_cast<void>(slim_edge_source->junction_owners(oriented(11, false), oriented(12, false)));
    const auto warm_edge_stats = slim_edge_source->stats();
    const auto lazy_edge = slim_edge_source->edge(oriented(11, false), oriented(12, false));
    const auto first_lazy_edge_stats = slim_edge_source->stats();
    const auto lazy_edge_again = slim_edge_source->edge(oriented(11, false), oriented(12, false));
    const auto second_lazy_edge_stats = slim_edge_source->stats();
    require(first_lazy_edge_stats.native_queries == warm_edge_stats.native_queries + 1 &&
                second_lazy_edge_stats.native_queries == first_lazy_edge_stats.native_queries &&
                lazy_edge.data() == lazy_edge_again.data(),
            "first explicit node-score edge getter must requery once and retain its worker vector");
    const auto* lazy_edge_data = lazy_edge.data();
    static_cast<void>(slim_edge_source->edge(oriented(9, true), oriented(10, true)));
    require(lazy_edge.data() == lazy_edge_data && same_edge_postings(lazy_edge,
                full_edge_source->edge(oriented(11, false), oriented(12, false))),
            "lazy node-score edge spans must survive other worker edge inserts");
    slim_edge_source->begin_read_group();
    require(slim_edge_source->stats().active_lease_bytes == 0,
            "node-score edge reset must release canonical and lazy edge leases");
    const auto before_reset_edge = slim_edge_source->stats();
    static_cast<void>(slim_edge_source->edge(oriented(11, false), oriented(12, false)));
    require(slim_edge_source->stats().native_queries == before_reset_edge.native_queries + 1,
            "post-reset explicit edge getter must reuse canonical evidence then requery raw postings once");
    const auto negative_edge = slim_edge_source->edge(static_cast<std::uint64_t>(-1),
                                                       static_cast<std::uint64_t>(-2));
    require(negative_edge.empty(), "negative-sentinel edge query must remain empty in node-score mode");

    auto cold_probe_source = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score, 4096,
        false);
    std::promise<void> cold_probe_start;
    const auto cold_probe_gate = cold_probe_start.get_future().share();
    std::array<std::future<bool>, 4> cold_probe_clients;
    for (auto& future : cold_probe_clients) {
        future = std::async(std::launch::async, [&] {
            cold_probe_gate.wait();
            return cold_probe_source->splice_targets(9, 10).empty();
        });
    }
    cold_probe_start.set_value();
    for (auto& future : cold_probe_clients) {
        require(future.get(), "concurrent cold probe clients must retain ordinary-body splice vetoes");
    }
    const auto cold_probe_stats = cold_probe_source->stats();
    require(cold_probe_stats.splice_cache.builds == 1 && cold_probe_stats.splice_probe_attempts == 1 &&
                cold_probe_stats.splice_probe_success == 1 && cold_probe_stats.active_lease_bytes == 0,
            "same-key cold probe clients must share one proof/cache build and release worker leases");

    auto cold_certificate_source = NativeSource::load(
        index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score);
    std::promise<void> cold_certificate_start;
    const auto cold_certificate_gate = cold_certificate_start.get_future().share();
    std::array<std::future<bool>, 4> cold_certificate_clients;
    for (auto& future : cold_certificate_clients) {
        future = std::async(std::launch::async, [&] {
            cold_certificate_gate.wait();
            return cold_certificate_source->splice_targets(9, 10).empty();
        });
    }
    cold_certificate_start.set_value();
    for (auto& future : cold_certificate_clients) {
        require(future.get(), "concurrent cold certificate clients must retain ordinary-body splice vetoes");
    }
    const auto cold_certificate_stats = cold_certificate_source->stats();
    require(cold_certificate_stats.splice_cache.builds == 1 &&
                cold_certificate_stats.splice_adjacency_checks == 1 &&
                cold_certificate_stats.splice_adjacency_success == 1 &&
                cold_certificate_stats.splice_probe_sa_steps == 0 &&
                cold_certificate_stats.active_lease_bytes == 0,
            "same-key cold certificate clients must share one adjacency proof and release worker leases");

    const auto malformed_index_path = temporary.path() / "native-malformed-edge.pci";
    std::filesystem::copy_file(index_path, malformed_index_path);
    {
        // PCANBND1's tenth (offset,size) pair identifies the model-reference
        // payload. Corrupt the copy before loading, never a live mapped index.
        std::fstream fixture(malformed_index_path, std::ios::in | std::ios::out | std::ios::binary);
        std::uint64_t refs_offset = 0;
        fixture.seekg(16 + 16 * 9);
        fixture.read(reinterpret_cast<char*>(&refs_offset), sizeof(refs_offset));
        require(static_cast<bool>(fixture), "cannot read malformed edge fixture section");
        fixture.seekp(static_cast<std::streamoff>(
            refs_offset + data.paths[6].model_first * sizeof(std::uint32_t)));
        const std::uint32_t invalid_model = 999999;
        fixture.write(reinterpret_cast<const char*>(&invalid_model), sizeof(invalid_model));
        fixture.flush();
        require(static_cast<bool>(fixture), "cannot prepare malformed edge fixture");
    }
    auto malformed_edge_source = NativeSource::load(
        malformed_index_path, xg_path, gbwt_path, r_index_path, 1, pancollapse::NativeQueryMode::node_score);
    const auto malformed_edge = [&] {
        try {
            static_cast<void>(malformed_edge_source->edge(oriented(11, false), oriented(12, false)));
        } catch (const std::runtime_error& error) {
            return std::string(error.what());
        }
        throw std::runtime_error("malformed edge model reference was accepted");
    };
    const std::string malformed_edge_error = malformed_edge();
    const auto malformed_after_first = malformed_edge_source->stats();
    require(malformed_edge() == malformed_edge_error &&
                malformed_edge_source->stats().edge_cache.builds == malformed_after_first.edge_cache.builds &&
                malformed_edge_source->stats().native_queries == malformed_after_first.native_queries + 1 &&
                malformed_edge_source->stats().active_lease_bytes == malformed_after_first.active_lease_bytes,
            "failed lazy edge postings must not publish a worker vector or rebuild canonical edge evidence");

    auto splice_source = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1);
    std::array<std::future<bool>, 8> shared_splice_queries;
    for (std::size_t index = 0; index < shared_splice_queries.size(); ++index) {
        shared_splice_queries[index] = std::async(std::launch::async, [&, index] {
            const auto targets = index % 2 == 0 ? splice_source->splice_targets(11, 12)
                                                : splice_source->splice_targets(12, 11);
            return targets.size() == 1 && targets[0] == 2;
        });
    }
    for (auto& future : shared_splice_queries) {
        require(future.get(), "concurrent workers must receive exact canonical splice targets");
    }
    const auto shared_splice_stats = splice_source->stats();
    require(shared_splice_stats.splice_cache.builds == 1 && shared_splice_stats.splice_cache.hits >= 7 &&
                shared_splice_stats.active_lease_bytes == 0,
            "same-key cross-worker splice construction must be single-flight and release leases");
    const auto retained_splice = splice_source->splice_targets(11, 12);
    const auto* retained_splice_data = retained_splice.data();
    static_cast<void>(splice_source->splice_targets(13, 14));
    static_cast<void>(splice_source->junction_owners(oriented(11, false), oriented(12, false)));
    require(retained_splice.data() == retained_splice_data && retained_splice.size() == 1 &&
                retained_splice[0] == 2 && splice_source->stats().active_lease_bytes > 0,
            "leased shared splice spans must survive later worker lookups within a read group");
    splice_source->begin_read_group();
    require(splice_source->stats().active_lease_bytes == 0,
            "source group reset must release shared splice leases");

    auto isolated_source = NativeSource::load(index_path, xg_path, gbwt_path, r_index_path, 1);
    require(isolated_source->stats().native_queries == 0 &&
                isolated_source->stats().node_cache.entries == 0,
            "different source instances must not share cached evidence");
    const auto invalid_node_error = [&] {
        try { static_cast<void>(isolated_source->node(0)); }
        catch (const std::runtime_error& error) { return std::string(error.what()); }
        throw std::runtime_error("native source accepted an invalid node");
    };
    const std::string initial_invalid_error = invalid_node_error();
    require(isolated_source->stats().node_cache.builds == 0 &&
                isolated_source->stats().node_cache.entries == 0,
            "failed source builders must publish no partial evidence");
    std::array<std::future<bool>, 8> shared_queries;
    for (auto& future : shared_queries) {
        future = std::async(std::launch::async, [&] {
            const auto* node = isolated_source->node(1);
            const auto postings = isolated_source->node_postings(*node);
            const auto edges = isolated_source->edge(oriented(1, false), oriented(2, false));
            return node->id == 1 && posting_for(postings, 0).flags == reused_flags &&
                edge_for(edges, 0).flags == reused_edge_flags;
        });
    }
    for (auto& future : shared_queries) {
        require(future.get(), "concurrent shared evidence changed a posting value");
    }
    const auto shared_stats = isolated_source->stats();
    require(shared_stats.node_cache.builds == 1 && shared_stats.edge_cache.builds == 1 &&
                shared_stats.native_queries == 2 && shared_stats.node_cache.hits >= 7 &&
                shared_stats.edge_cache.hits >= 7,
            "same-key concurrent node/edge construction must be single-flight");
    require(shared_stats.active_lease_bytes == 0 &&
                shared_stats.active_lease_high_water_bytes > 0,
            "thread-local destruction must release worker leases after join");
    require(shared_stats.node_cache.resident_bytes <= shared_stats.node_cache.capacity_bytes &&
                shared_stats.edge_cache.resident_bytes <= shared_stats.edge_cache.capacity_bytes &&
                shared_stats.node_cache.maximum_entry_bytes > 0,
            "source evidence cache byte accounting must remain bounded and observable");
    require(invalid_node_error() == initial_invalid_error &&
                isolated_source->stats().node_cache.builds == 1,
            "cache population must not change invalid-node faults or cache failed builds");
    static_cast<void>(isolated_source->node(1));
    require(isolated_source->stats().active_lease_bytes > 0,
            "source-destruction fixture must hold an active current-thread lease");
    isolated_source.reset();

    std::cout << "native count source query evidence checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
