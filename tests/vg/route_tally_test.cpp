// D091 route tally: each HST's score is its best single alignment path. Checked against brute-force
// enumeration of every source-to-sink path on random subpath graphs.

#include "pathtally.hpp"

#include <vg/vg.pb.h>

#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

using Steps = std::map<int64_t, std::vector<std::pair<std::string, bool>>>;  // node -> (hst, rev)
using Scores = std::map<const vg::MultipathAlignment*, std::vector<std::vector<int64_t>>>;

pathtally::NodeScorer table_scorer(const Scores& scores) {
    return [&scores](const vg::MultipathAlignment& mp, int subpath, size_t, std::vector<int64_t>& out) {
        out = scores.at(&mp).at(static_cast<size_t>(subpath));
    };
}

std::map<std::string, pathtally::RouteScore> run_route(
    const std::vector<const vg::MultipathAlignment*>& records, const Scores& scores,
    const Steps& steps) {
    pathtally::RouteTally<std::string> tally;
    std::map<std::string, pathtally::RouteScore> result;
    tally.run(
        records, table_scorer(scores),
        [&](int64_t node, const auto& emit) {
            const auto found = steps.find(node);
            if (found == steps.end()) return;
            for (const auto& [hst, reverse] : found->second) emit(hst, reverse);
        },
        [&](const std::string& hst, const pathtally::RouteScore& best) {
            check(result.emplace(hst, best).second, "HST emitted twice");
        });
    return result;
}

// Every source-to-sink path, scored per HST that the path visits.
std::map<std::string, pathtally::RouteScore> brute_force(
    const std::vector<const vg::MultipathAlignment*>& records, const Scores& scores,
    const Steps& steps) {
    std::map<std::string, pathtally::RouteScore> best;
    for (const vg::MultipathAlignment* record : records) {
        const int count = record->subpath_size();
        std::vector<std::vector<int>> next(static_cast<size_t>(count));
        std::vector<bool> has_in(static_cast<size_t>(count), false);
        for (int s = 0; s < count; ++s) {
            for (const uint32_t n : record->subpath(s).next()) {
                next[static_cast<size_t>(s)].push_back(static_cast<int>(n));
                has_in[n] = true;
            }
            for (const vg::Connection& c : record->subpath(s).connection()) {
                next[static_cast<size_t>(s)].push_back(static_cast<int>(c.next()));
                has_in[c.next()] = true;
            }
        }
        std::vector<int> sources;
        if (record->start_size() > 0) {
            for (const uint32_t start : record->start()) sources.push_back(static_cast<int>(start));
        } else {
            for (int s = 0; s < count; ++s) if (!has_in[static_cast<size_t>(s)]) sources.push_back(s);
        }
        std::vector<int> route;
        auto score_route = [&] {
            std::map<std::string, pathtally::RouteScore> totals;
            for (const int s : route) {
                const vg::Path& path = record->subpath(s).path();
                for (int m = 0; m < path.mapping_size(); ++m) {
                    const vg::Mapping& mapping = path.mapping(m);
                    const auto found = steps.find(mapping.position().node_id());
                    if (found == steps.end()) continue;
                    const int64_t score = scores.at(record)[static_cast<size_t>(s)][static_cast<size_t>(m)];
                    const int64_t bases = pathtally::mapping_aligned_bases(mapping);
                    for (const auto& [hst, reverse] : found->second) {
                        pathtally::RouteScore& total = totals[hst];
                        total.score += score;
                        (mapping.position().is_reverse() == reverse ? total.forward_bases
                                                                    : total.reverse_bases) += bases;
                    }
                }
            }
            for (const auto& [hst, total] : totals) {
                const auto found = best.find(hst);
                if (found == best.end() || pathtally::route_score_better(total, found->second)) {
                    best[hst] = total;
                }
            }
        };
        auto walk = [&](auto&& self, int s) -> void {
            route.push_back(s);
            if (next[static_cast<size_t>(s)].empty()) score_route();
            for (const int n : next[static_cast<size_t>(s)]) self(self, n);
            route.pop_back();
        };
        for (const int source : sources) walk(walk, source);
    }
    return best;
}

void add_mapping(vg::Subpath* subpath, int64_t node, bool reverse, int bases) {
    vg::Mapping* mapping = subpath->mutable_path()->add_mapping();
    mapping->mutable_position()->set_node_id(node);
    mapping->mutable_position()->set_is_reverse(reverse);
    vg::Edit* edit = mapping->add_edit();
    edit->set_from_length(bases);
    edit->set_to_length(bases);
}

void expect_equal(const std::map<std::string, pathtally::RouteScore>& got,
                  const std::map<std::string, pathtally::RouteScore>& want, const std::string& label) {
    check(got.size() == want.size(), label + ": HST sets differ");
    for (const auto& [hst, score] : want) {
        const auto found = got.find(hst);
        check(found != got.end(), label + ": missing " + hst);
        check(found->second.score == score.score && found->second.forward_bases == score.forward_bases &&
                  found->second.reverse_bases == score.reverse_bases,
              label + ": " + hst + " score " + std::to_string(found->second.score) + " want " +
                  std::to_string(score.score));
    }
}

// The explanation's example: A, then three alternatives through X for the next stretch, then C.
// Summing credited X three times (T = 40); the best single path gives T 28.
void test_alternatives_are_not_summed() {
    vg::MultipathAlignment read;
    add_mapping(read.add_subpath(), 1, false, 10);  // A
    for (const int64_t detour : {11, 12, 13}) {       // p, q, r
        vg::Subpath* alternative = read.add_subpath();
        add_mapping(alternative, 2, false, 6);          // X
        add_mapping(alternative, detour, false, 2);
    }
    add_mapping(read.add_subpath(), 3, false, 10);  // C
    for (const uint32_t alt : {1U, 2U, 3U}) {
        read.mutable_subpath(0)->add_next(alt);
        read.mutable_subpath(static_cast<int>(alt))->add_next(4);
    }
    Scores scores{{&read, {{10}, {6, 2}, {6, 1}, {6, -1}, {10}}}};
    Steps steps{{1, {{"T", false}}}, {2, {{"T", false}}}, {11, {{"T", false}}}, {3, {{"T", false}}},
                {12, {{"U", false}}}};
    const auto got = run_route({&read}, scores, steps);
    check(got.at("T").score == 28, "T must score its best path, 28");
    // U is visited only on the q path; that path gives U just q's +1.
    check(got.at("U").score == 1, "U must score only the path that visits it");
    expect_equal(got, brute_force({&read}, scores, steps), "alternatives");
}

// Separate records of one read are alternatives too: the best one wins, not their sum.
void test_records_are_not_summed() {
    vg::MultipathAlignment first, second;
    add_mapping(first.add_subpath(), 1, false, 20);
    add_mapping(second.add_subpath(), 1, false, 20);
    Scores scores{{&first, {{20}}}, {&second, {{15}}}};
    Steps steps{{1, {{"T", false}}}};
    const auto got = run_route({&first, &second}, scores, steps);
    check(got.at("T").score == 20, "records must not be summed");
}

void test_random_graphs_match_brute_force() {
    std::mt19937 rng(20260923);
    const std::vector<std::string> hsts{"T1", "T2", "T3", "T4", "T5"};
    for (int trial = 0; trial < 3000; ++trial) {
        Steps steps;
        for (int64_t node = 1; node <= 7; ++node) {
            for (const std::string& hst : hsts) {
                const unsigned roll = rng() % 10;
                if (roll < 4) steps[node].push_back({hst, rng() % 4 == 0});
                if (roll == 0) steps[node].push_back({hst, rng() % 2 == 0});  // a path visiting twice
            }
        }
        std::vector<vg::MultipathAlignment> reads(1 + rng() % 3);
        Scores scores;
        std::vector<const vg::MultipathAlignment*> records;
        for (vg::MultipathAlignment& read : reads) {
            const int count = 1 + static_cast<int>(rng() % 9);
            std::vector<std::vector<int64_t>> table;
            for (int s = 0; s < count; ++s) {
                vg::Subpath* subpath = read.add_subpath();
                std::vector<int64_t> row;
                const int mappings = 1 + static_cast<int>(rng() % 3);
                for (int m = 0; m < mappings; ++m) {
                    add_mapping(subpath, 1 + static_cast<int64_t>(rng() % 9), rng() % 5 == 0,
                                1 + static_cast<int>(rng() % 5));
                    row.push_back(static_cast<int64_t>(rng() % 13) - 5);
                }
                table.push_back(row);
            }
            for (int s = 0; s + 1 < count; ++s) {
                bool linked = false;
                for (int n = s + 1; n < count; ++n) {
                    if (rng() % 3 == 0) {
                        if (rng() % 4 == 0) {
                            vg::Connection* connection = read.mutable_subpath(s)->add_connection();
                            connection->set_next(static_cast<uint32_t>(n));
                            connection->set_score(static_cast<int32_t>(rng() % 5));
                        } else {
                            read.mutable_subpath(s)->add_next(static_cast<uint32_t>(n));
                        }
                        linked = true;
                    }
                }
                if (!linked && rng() % 2 == 0) read.mutable_subpath(s)->add_next(static_cast<uint32_t>(s + 1));
            }
            if (rng() % 3 == 0) {
                read.add_start(0);
                if (count > 1 && rng() % 2 == 0) read.add_start(static_cast<uint32_t>(count - 1));
            }
            scores[&read] = table;
            records.push_back(&read);
        }
        expect_equal(run_route(records, scores, steps), brute_force(records, scores, steps),
                     "trial " + std::to_string(trial));
    }
}

// Bases beyond the packed-key range force the exact program; the answer must not change.
void test_large_values_use_the_exact_program() {
    vg::MultipathAlignment read;
    add_mapping(read.add_subpath(), 1, false, 700000);
    add_mapping(read.add_subpath(), 2, false, 700000);
    add_mapping(read.add_subpath(), 3, false, 700000);
    read.mutable_subpath(0)->add_next(1);
    read.mutable_subpath(0)->add_next(2);
    Scores scores{{&read, {{600000}, {5}, {7}}}};
    Steps steps{{1, {{"T", false}}}, {2, {{"T", false}}}, {3, {{"U", false}}}};
    const auto got = run_route({&read}, scores, steps);
    expect_equal(got, brute_force({&read}, scores, steps), "large values");
    check(got.at("T").forward_bases == 1400000, "large bases must survive exactly");
}

void test_out_of_order_subpaths_are_rejected() {
    vg::MultipathAlignment read;
    add_mapping(read.add_subpath(), 1, false, 5);
    add_mapping(read.add_subpath(), 2, false, 5);
    read.mutable_subpath(1)->add_next(0);
    Scores scores{{&read, {{5}, {5}}}};
    bool threw = false;
    try {
        run_route({&read}, scores, Steps{{1, {{"T", false}}}});
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("topological") != std::string::npos;
    }
    check(threw, "a backward subpath edge must be rejected");
}

}  // namespace

int main() {
    try {
        test_alternatives_are_not_summed();
        test_records_are_not_summed();
        test_random_graphs_match_brute_force();
        pathtally::packed_route_keys_enabled() = false;  // the same checks on the exact program
        test_alternatives_are_not_summed();
        test_records_are_not_summed();
        test_random_graphs_match_brute_force();
        pathtally::packed_route_keys_enabled() = true;
        test_large_values_use_the_exact_program();
        test_out_of_order_subpaths_are_rejected();
    } catch (const std::exception& error) {
        std::cerr << "route_tally_test: " << error.what() << '\n';
        return 1;
    }
    std::cout << "route_tally_test: ok\n";
    return 0;
}
