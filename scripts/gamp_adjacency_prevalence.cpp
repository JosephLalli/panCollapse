#include "gamp_alignment_complexity.hpp"
#include "read_granular_input.hpp"

#include <gbwt/algorithms.h>
#include <gbwt/dynamic_gbwt.h>
#include <gbwt/gbwt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using OrientedPair = std::pair<gbwt::node_type, gbwt::node_type>;
using PhysicalPair = std::pair<std::uint64_t, std::uint64_t>;

struct StopReading final : std::exception {};
struct Summary {
    std::uint64_t groups = 0, records = 0, cutoff_drops = 0, weighted_requests = 0;
    std::uint64_t same_physical_noops = 0, certified_weighted = 0;
    std::map<PhysicalPair, std::uint64_t> pairs;
    std::array<std::uint64_t, 16> success_hist{};
    std::array<std::uint64_t, 4> all_edges{}, certified_edges{};
};

void add(std::uint64_t& value, std::uint64_t amount = 1) {
    if (amount > std::numeric_limits<std::uint64_t>::max() - value) throw std::runtime_error("uint64 aggregate overflow");
    value += amount;
}

std::uint64_t limit(const char* value) {
    std::size_t consumed = 0; const auto result = std::stoull(value, &consumed);
    if (value[0] == '-' || consumed != std::string(value).size()) throw std::runtime_error("GROUPS must be a non-negative integer");
    return result;
}

// A direct Record::outgoing test is intentionally paired with exact FM searches below.
bool outgoing_all_to(const gbwt::GBWT& index, gbwt::node_type from, std::uint64_t other) {
    if (!index.contains(from)) return false;
    const auto record = index.record(from);
    if (record.outgoing.empty()) return false;
    for (const auto next : record.outgoing)
        if (next.first == gbwt::ENDMARKER || gbwt::Node::id(next.first) != other) return false;
    return true;
}

bool exact_all_to(const gbwt::GBWT& index, gbwt::node_type from, std::uint64_t other) {
    if (!index.contains(from)) return false;
    const auto whole = gbwt::find(index, from);
    if (whole.empty()) return false;
    const auto forward = gbwt::extend(index, whole, gbwt::Node::encode(other, false));
    const auto reverse = gbwt::extend(index, whole, gbwt::Node::encode(other, true));
    return forward.size() + reverse.size() == whole.size();
}

std::uint64_t occurrences(const gbwt::GBWT& index, gbwt::node_type from, gbwt::node_type to) {
    if (!index.contains(from)) return 0;
    return gbwt::extend(index, gbwt::find(index, from), to).size();
}

std::uint8_t certify(const gbwt::GBWT& index, const PhysicalPair& pair) {
    const std::array<gbwt::node_type, 2> left = {gbwt::Node::encode(pair.first, false), gbwt::Node::encode(pair.first, true)};
    const std::array<gbwt::node_type, 2> right = {gbwt::Node::encode(pair.second, false), gbwt::Node::encode(pair.second, true)};
    std::uint8_t mask = 0;
    // The four anchors are aF, aR, bF, bR.  The target is the other physical
    // endpoint, so the predicate intentionally does not choose one target orientation.
    for (std::size_t anchor = 0; anchor < 4; ++anchor) {
        const auto from = anchor < 2 ? left[anchor] : right[anchor - 2];
        const auto other = anchor < 2 ? pair.second : pair.first;
        const auto bit = static_cast<std::uint8_t>(1U << anchor);
        const bool direct = outgoing_all_to(index, from, other);
        const bool exact = exact_all_to(index, from, other);
        if (direct != exact) throw std::runtime_error("GBWT outgoing predicate disagrees with find/extend cross-check");
        if (direct) mask |= bit;
    }
    return mask;
}

struct Piece { std::optional<gbwt::node_type> first, last; };

// This is the resolver's topology rule reduced to endpoints: internal mapping joins are
// collected for every piece, while empty pieces propagate reachable predecessor endpoints.
std::set<OrientedPair> record_pairs(const vg::MultipathAlignment& alignment) {
    const auto n = static_cast<std::size_t>(alignment.subpath_size());
    std::vector<Piece> pieces(n); std::vector<bool> incoming(n), reachable(n);
    std::set<OrientedPair> result;
    for (std::size_t i = 0; i < n; ++i) {
        const auto& subpath = alignment.subpath(static_cast<int>(i));
        for (int m = 0; m < subpath.path().mapping_size(); ++m) {
            const auto& mapping = subpath.path().mapping(m);
            // A zero-length mapping cannot consume an adjacency endpoint.
            std::uint64_t bases = 0; for (const auto& edit : mapping.edit()) bases += edit.from_length();
            if (!bases) continue;
            const auto node = gbwt::Node::encode(static_cast<std::uint64_t>(mapping.position().node_id()), mapping.position().is_reverse());
            if (!pieces[i].first) pieces[i].first = node;
            if (pieces[i].last) result.emplace(*pieces[i].last, node);
            pieces[i].last = node;
        }
        const auto mark = [&](std::uint32_t next) {
            if (next >= n || next <= i) throw std::runtime_error("GAMP subpaths are not topologically ordered");
            incoming[next] = true;
        };
        for (const auto next : subpath.next()) mark(next);
        for (const auto& edge : subpath.connection()) mark(edge.next());
    }
    std::vector<std::uint32_t> starts(alignment.start().begin(), alignment.start().end());
    if (starts.empty()) for (std::uint32_t i = 0; i < n; ++i) if (!incoming[i]) starts.push_back(i);
    for (const auto start : starts) { if (start >= n) throw std::runtime_error("GAMP start index is out of range"); reachable[start] = true; }
    std::vector<std::vector<gbwt::node_type>> endpoints(n);
    for (std::size_t i = 0; i < n; ++i) if (reachable[i]) {
        auto previous = std::move(endpoints[i]); std::sort(previous.begin(), previous.end()); previous.erase(std::unique(previous.begin(), previous.end()), previous.end());
        if (pieces[i].first) for (const auto last : previous) result.emplace(last, *pieces[i].first);
        if (pieces[i].last) previous = {*pieces[i].last};
        const auto send = [&](std::uint32_t next) { reachable[next] = true; endpoints[next].insert(endpoints[next].end(), previous.begin(), previous.end()); };
        const auto& subpath = alignment.subpath(static_cast<int>(i));
        for (const auto next : subpath.next()) send(next);
        for (const auto& edge : subpath.connection()) send(edge.next());
    }
    return result;
}

void process_group(std::vector<std::string>& bytes, Summary& summary, pancollapse::input::GampAlignmentComplexity& complexity) {
    add(summary.groups);
    std::vector<vg::MultipathAlignment> records(bytes.size()); std::vector<const vg::MultipathAlignment*> views;
    for (std::size_t i = 0; i < bytes.size(); ++i) { pancollapse::input::decode_multipath_record(bytes[i], records[i]); views.push_back(&records[i]); }
    if (complexity.exceeds(views, pancollapse::input::kDefaultMaxAlignmentsPerRead)) { add(summary.cutoff_drops); return; }
    std::set<OrientedPair> oriented;
    for (const auto& record : records) { const auto one = record_pairs(record); oriented.insert(one.begin(), one.end()); }
    for (const auto& [first, second] : oriented) {
        const auto a = gbwt::Node::id(first), b = gbwt::Node::id(second);
        if (a == b) { add(summary.same_physical_noops); continue; }
        add(summary.weighted_requests); add(summary.pairs[{std::min(a, b), std::max(a, b)}]);
    }
}

void self_test() {
    gbwt::GBWTBuilder builder(8, 64, 1);
    const auto insert = [&](std::initializer_list<std::pair<std::uint64_t, bool>> nodes) {
        gbwt::vector_type walk;
        for (const auto& [id, reverse] : nodes) walk.push_back(gbwt::Node::encode(id, reverse));
        builder.insert(walk, true);
    };
    insert({{1, false}, {2, false}}); // repeated node and mixed target orientations remain physical 1/2.
    insert({{1, false}, {2, true}});
    insert({{1, false}, {2, false}, {1, false}, {2, false}}); // repeated occurrences, not just duplicate paths.
    insert({{2, false}, {9, false}}); // extra occurrences of the other endpoint do not matter.
    insert({{11, false}, {10, false}}); // reverse physical order, tested separately.
    insert({{3, false}}); // endmarker defeats 3 -> anything.
    insert({{4, false}, {5, false}, {6, false}});
    insert({{4, false}, {7, false}}); // branch defeats 4 -> 5.
    insert({{8, false}, {5, false}}); // extra other predecessor defeats the reverse 5 -> 4 anchor.
    builder.finish(); gbwt::GBWT index(builder.index);
    if (certify(index, {1, 2}) == 0) throw std::runtime_error("self-test expected 1/2 certification");
    if (certify(index, {10, 11}) == 0) throw std::runtime_error("self-test reverse-order certification");
    if (certify(index, {3, 4}) != 0 || certify(index, {4, 5}) != 0) throw std::runtime_error("self-test endmarker/branch failure");
    std::mt19937 random(0xAD1AC);
    for (int trial = 0; trial < 32; ++trial) {
        gbwt::GBWTBuilder random_builder(8, 64, 1);
        for (int path = 0; path < 8; ++path) {
            gbwt::vector_type walk;
            const auto length = 1 + random() % 8;
            for (std::uint32_t i = 0; i < length; ++i)
                walk.push_back(gbwt::Node::encode(1 + random() % 8, random() % 2));
            random_builder.insert(walk, true);
        }
        random_builder.finish();
        gbwt::GBWT random_index(random_builder.index);
        for (std::uint64_t from = 1; from <= 8; ++from)
            for (std::uint64_t to = from + 1; to <= 8; ++to)
                (void)certify(random_index, {from, to});
    }
    vg::MultipathAlignment alignment; auto* a = alignment.add_subpath(); auto* am = a->mutable_path()->add_mapping(); am->mutable_position()->set_node_id(1); am->add_edit()->set_from_length(1);
    auto* empty = alignment.add_subpath(); auto* b = alignment.add_subpath(); auto* bm = b->mutable_path()->add_mapping(); bm->mutable_position()->set_node_id(2); bm->add_edit()->set_from_length(1);
    a->add_next(1); empty->add_connection()->set_next(2); alignment.add_start(0);
    if (!record_pairs(alignment).contains({gbwt::Node::encode(1, false), gbwt::Node::encode(2, false)})) throw std::runtime_error("self-test empty bridge failure");
}
} // namespace

int main(int argc, char** argv) try {
    if (argc == 2 && std::string(argv[1]) == "--self-test") { self_test(); return 0; }
    if (argc != 3 && argc != 4) throw std::runtime_error("usage: pancollapse_gamp_adjacency_prevalence GBWT GAMP [GROUPS]");
    const std::uint64_t group_limit = argc == 4 ? limit(argv[3]) : 10000;
    const auto began = std::chrono::steady_clock::now(); std::ifstream gbwt_input(argv[1], std::ios::binary); if (!gbwt_input) throw std::runtime_error("cannot open GBWT");
    gbwt::GBWT index; index.load(gbwt_input); if (!index.bidirectional()) throw std::runtime_error("GBWT must be bidirectional"); index.clearMetadata();
    const double load_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    std::ifstream input(argv[2], std::ios::binary); if (!input) throw std::runtime_error("cannot open GAMP input");
    Summary summary; pancollapse::input::GampAlignmentComplexity complexity; std::string name; std::vector<std::string> group; bool have = false;
    const auto scan_began = std::chrono::steady_clock::now();
    if (group_limit != 0) { try { pancollapse::input::raw_gamp_for_each(input, [&](std::unique_ptr<std::string> raw) {
        pancollapse::input::MultipathRecordPeek peek; std::optional<vg::MultipathAlignment> decoded;
        if (!pancollapse::input::peek_multipath_record(*raw, peek)) { decoded.emplace(); pancollapse::input::decode_multipath_record(*raw, *decoded); peek.name = decoded->name(); peek.has_subpath = decoded->subpath_size() != 0; }
        if (have && peek.name != name) { process_group(group, summary, complexity); group.clear(); have = false; if (summary.groups == group_limit) throw StopReading{}; }
        if (!have) { name.assign(peek.name); have = true; }
        add(summary.records); if (peek.has_subpath) group.push_back(std::move(*raw));
    }, 2); } catch (const StopReading&) {} }
    if (have && summary.groups < group_limit) process_group(group, summary, complexity);
    const double scan_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - scan_began).count(); const auto certify_began = std::chrono::steady_clock::now();
    for (const auto& [pair, weight] : summary.pairs) {
        const auto mask = certify(index, pair); add(summary.success_hist[mask]);
        const std::array<gbwt::node_type, 2> left = {gbwt::Node::encode(pair.first, false), gbwt::Node::encode(pair.first, true)};
        const std::array<gbwt::node_type, 2> right = {gbwt::Node::encode(pair.second, false), gbwt::Node::encode(pair.second, true)};
        for (std::size_t li = 0; li < 2; ++li) for (std::size_t ri = 0; ri < 2; ++ri) {
            const auto bit = li * 2 + ri, count = occurrences(index, left[li], right[ri]);
            add(summary.all_edges[bit], count);
            if (mask != 0) add(summary.certified_edges[bit], count);
        }
        if (mask) add(summary.certified_weighted, weight);
    }
    const double certify_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - certify_began).count();
    std::uint64_t certified_unique = 0;
    for (std::size_t mask = 1; mask < summary.success_hist.size(); ++mask) add(certified_unique, summary.success_hist[mask]);
    std::cout << "{\"schema\":\"pancollapse-gamp-adjacency-prevalence-v1\",\"denominator\":\"structural query stream/superset; not actual count work\",\"groups\":" << summary.groups << ",\"records\":" << summary.records << ",\"cutoff_drops\":" << summary.cutoff_drops << ",\"uniquephysicalpairs\":" << summary.pairs.size() << ",\"weightedrequests\":" << summary.weighted_requests << ",\"certifiedunique\":" << certified_unique << ",\"certifiedweighted\":" << summary.certified_weighted << ",\"successanchorhist\":["; for (std::size_t i=0;i<16;++i) std::cout << (i ? "," : "") << summary.success_hist[i]; std::cout << "],\"all4edgeoccurrences\":["; for (std::size_t i=0;i<4;++i) std::cout << (i ? "," : "") << summary.all_edges[i]; std::cout << "],\"certifiededgeoccurrences\":["; for (std::size_t i=0;i<4;++i) std::cout << (i ? "," : "") << summary.certified_edges[i]; std::cout << "],\"timings\":{\"load\":" << load_seconds << ",\"scan\":" << scan_seconds << ",\"certify\":" << certify_seconds << "}}\n";
} catch (const std::exception& error) { std::cerr << "pancollapse_gamp_adjacency_prevalence: " << error.what() << '\n'; return 1; }
