#include "gamp_alignment_complexity.hpp"

#include <vg/vg.pb.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using pancollapse::input::GampAlignmentComplexity;

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

bool exceeds(GampAlignmentComplexity& complexity, const std::vector<vg::MultipathAlignment>& values,
             std::uint64_t limit) {
    std::vector<const vg::MultipathAlignment*> pointers;
    for (const auto& value : values) pointers.push_back(&value);
    return complexity.exceeds(pointers, limit);
}

vg::MultipathAlignment graph(int nodes, const std::vector<std::pair<int, int>>& edges,
                             const std::vector<int>& starts = {}) {
    vg::MultipathAlignment result;
    for (int i = 0; i < nodes; ++i) result.add_subpath();
    for (const auto& [from, to] : edges) result.mutable_subpath(from)->add_next(to);
    for (const int start : starts) result.add_start(start);
    return result;
}

std::uint64_t brute(const vg::MultipathAlignment& value) {
    const int count = value.subpath_size();
    std::vector<bool> incoming(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        for (const auto next : value.subpath(i).next()) incoming[next] = true;
        for (const auto& connection : value.subpath(i).connection()) incoming[connection.next()] = true;
    }
    auto walk = [&](auto&& self, int node) -> std::uint64_t {
        std::uint64_t paths = 0;
        bool has_next = false;
        for (const auto next : value.subpath(node).next()) { has_next = true; paths += self(self, next); }
        for (const auto& connection : value.subpath(node).connection()) {
            has_next = true; paths += self(self, connection.next());
        }
        return has_next ? paths : 1;
    };
    std::uint64_t paths = 0;
    if (value.start_size()) for (const auto start : value.start()) paths += walk(walk, start);
    else for (int i = 0; i < count; ++i) if (!incoming[static_cast<size_t>(i)]) paths += walk(walk, i);
    return paths;
}

void expect_routes(const vg::MultipathAlignment& value, std::uint64_t routes) {
    GampAlignmentComplexity complexity;
    for (std::uint64_t limit = 1; limit <= routes + 1; ++limit) {
        check(exceeds(complexity, {value}, limit) == (routes > limit), "route count mismatch");
    }
}

void test_basics() {
    check(pancollapse::input::kDefaultMaxAlignmentsPerRead == 64000000, "default cutoff");
    GampAlignmentComplexity complexity;
    check(!exceeds(complexity, {}, 1), "empty group");
    check(!exceeds(complexity, {graph(0, {})}, 1), "empty record");
    expect_routes(graph(3, {{0, 1}, {1, 2}}), 1);
    expect_routes(graph(4, {{0, 1}, {0, 2}, {1, 3}, {2, 3}}), 2);
    expect_routes(graph(3, {{0, 1}, {0, 1}, {1, 2}}), 2);
    auto connections = graph(3, {{0, 1}}); connections.mutable_subpath(0)->add_connection()->set_next(1);
    connections.mutable_subpath(1)->add_connection()->set_next(2);
    expect_routes(connections, 2);
    expect_routes(graph(3, {{0, 2}, {1, 2}}), 2);
    expect_routes(graph(4, {{0, 3}, {1, 3}, {2, 3}}, {0, 0, 1}), 3);
    expect_routes(graph(4, {{0, 1}, {2, 3}}), 2);
    check(!exceeds(complexity, {graph(2, {{0, 1}})}, 0), "zero disables filtering");
    check(!exceeds(complexity, {graph(3, {{0, 1}, {0, 2}})}, 2), "exact cutoff retained");
    check(exceeds(complexity, {graph(3, {{0, 1}, {0, 2}})}, 1), "cutoff plus one dropped");
    check(exceeds(complexity, {graph(3, {{0, 1}, {0, 2}}), graph(3, {{0, 1}, {0, 2}})}, 3),
          "records sum per QNAME");
}

void test_large_and_errors() {
    GampAlignmentComplexity complexity;
    vg::MultipathAlignment huge;
    for (int i = 0; i < 201; ++i) huge.add_subpath();
    for (int i = 0; i < 200; ++i) { huge.mutable_subpath(i)->add_next(i + 1); huge.mutable_subpath(i)->add_next(i + 1); }
    check(exceeds(complexity, {huge}, 100), "2^100 routes capped without enumeration");
    check(!exceeds(complexity, {graph(2, {{0, 1}})}, std::numeric_limits<std::uint64_t>::max() - 1),
          "near max retains one route");
    check(exceeds(complexity, {huge}, std::numeric_limits<std::uint64_t>::max() - 1),
          "2^100 routes saturate at the near-max cutoff");
    bool threw = false;
    try { exceeds(complexity, {graph(1, {})}, std::numeric_limits<std::uint64_t>::max()); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "UINT64_MAX rejected");
    auto outside = graph(1, {}); outside.mutable_subpath(0)->add_next(1); threw = false;
    try { exceeds(complexity, {outside}, 1); } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "out-of-range edge rejected");
    auto backwards = graph(2, {{1, 0}}); threw = false;
    try { exceeds(complexity, {backwards}, 1); } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "non-topological edge rejected");
    check(exceeds(complexity, {graph(3, {{0, 1}, {0, 2}})}, 1), "scratch reused after exceptions");
}

void test_random_tiny_dags() {
    std::mt19937 rng(71123);
    GampAlignmentComplexity complexity;
    for (int trial = 0; trial < 100; ++trial) {
        const int nodes = 1 + rng() % 7;
        vg::MultipathAlignment value = graph(nodes, {});
        for (int from = 0; from < nodes; ++from) for (int to = from + 1; to < nodes; ++to) {
            if (rng() % 3 == 0) value.mutable_subpath(from)->add_next(to);
            if (rng() % 8 == 0) value.mutable_subpath(from)->add_connection()->set_next(to);
        }
        const auto exact = brute(value);
        for (std::uint64_t limit = 0; limit <= 20; ++limit) {
            check(exceeds(complexity, {value}, limit) == (limit != 0 && exact > limit), "random DAG mismatch");
        }
    }
}

}  // namespace

int main() {
    try {
        test_basics();
        test_large_and_errors();
        test_random_tiny_dags();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
