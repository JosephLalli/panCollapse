#include "score_dedup.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

struct State {
    int identity = 0;
    std::int64_t score = 0;
    bool operator==(const State&) const = default;
};

int main() {
    pancollapse::StableScoreDedup scratch;
    std::mt19937 random(3917);
    for (int trial = 0; trial < 2000; ++trial) {
        std::vector<State> states;
        std::vector<int> first_order;
        std::map<int, std::int64_t> maxima;
        const auto size = trial < 3 ? static_cast<unsigned>(trial) : random() % 400;
        for (unsigned i = 0; i < size; ++i) {
            const int key = static_cast<int>(random() % 101);
            const auto score = static_cast<std::int64_t>(random() % 1000) - 500;
            states.push_back({key, score});
            const auto [found, inserted] = maxima.try_emplace(key, score);
            if (inserted) first_order.push_back(key);
            else found->second = std::max(found->second, score);
        }
        const auto original = states;
        // All-colliding hashes exercise probing and in-place compaction after a
        // duplicate; ordinary hashes exercise changing scratch-table sizes.
        scratch.retain_best(states,
            [trial](const State& s) {
                return trial % 2 ? std::size_t{0} : std::hash<int>{}(s.identity);
            },
            [](const State& a, const State& b) { return a.identity == b.identity; });
        std::vector<State> expected;
        for (const auto key : first_order) expected.push_back({key, maxima.at(key)});
        if (states != expected) throw std::runtime_error("stable identity/max score mismatch");
        auto evidence = original;
        pancollapse::sort_best_evidence(evidence,
            [](const State& a, const State& b) { return a.identity < b.identity; });
        expected.clear();
        for (const auto& [key, score] : maxima) expected.push_back({key, score});
        if (evidence != expected) throw std::runtime_error("ordered evidence/max score mismatch");
        // Split records with overlapping identities must produce the same group.
        auto left = std::vector<State>(original.begin(), original.begin() + original.size() / 2);
        auto right = std::vector<State>(original.begin() + original.size() / 2, original.end());
        const auto less = [](const State& a, const State& b) { return a.identity < b.identity; };
        pancollapse::sort_best_evidence(left, less);
        pancollapse::sort_best_evidence(right, less);
        left.insert(left.end(), right.begin(), right.end());
        pancollapse::sort_best_evidence(left, less);
        if (left != expected) throw std::runtime_error("cross-record evidence mismatch");
    }
    std::cout << "PASS: stable/max-score, collisions, reuse, ordered and cross-record evidence\n";
}
