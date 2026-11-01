#include "compact_score_frontier.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

struct Key {
    int branch = 0, transcript = 0, state = 0;
    bool operator==(const Key&) const = default;
    bool operator<(const Key& other) const {
        return std::tie(branch, transcript, state) <
               std::tie(other.branch, other.transcript, other.state);
    }
};

using Frontier = pancollapse::CompactScoreFrontier<Key>;
using Golden = std::map<Key, std::pair<std::int64_t, std::int64_t>>;

void require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}

void check_bounds(const Frontier& frontier, std::size_t distinct, std::size_t batch) {
    const auto scale = std::max(distinct, batch);
    require(frontier.pending_cells() <= 2 * scale,
            "pending frontier cells must remain bounded by compacted distinct states");
    // This observes the allocation behavior of the supported STL; exact
    // max-plus semantics do not depend on vector reserve over-allocation.
    require(frontier.capacity() <= 4 * scale,
            "supported-STL allocation regression exceeded frontier capacity bound");
}

void append(Frontier& frontier, Golden& golden, const Key& key,
            std::int64_t score, std::int64_t gene_score, std::size_t batch) {
    frontier.add(key, score, gene_score);
    const auto [found, inserted] = golden.emplace(key, std::pair{score, gene_score});
    if (!inserted) {
        found->second.first = std::max(found->second.first, score);
        found->second.second = std::max(found->second.second, gene_score);
    }
    check_bounds(frontier, golden.size(), batch);
}

void check_take(Frontier& frontier, const Golden& golden) {
    const auto cells = frontier.take_sorted();
    require(cells.size() == golden.size(), "take must retain exactly one cell per full key");
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto expected = std::next(golden.begin(), static_cast<std::ptrdiff_t>(i));
        require(cells[i].key == expected->first && cells[i].score == expected->second.first &&
                    cells[i].gene_score == expected->second.second,
                "take must sort full keys and preserve independent maxima");
    }
    require(frontier.pending_cells() == 0, "take must empty the frontier");
}

std::uint32_t next_random(std::uint32_t& state) {
    state = state * 1664525U + 1013904223U;
    return state;
}

void duplicate_fanin(std::size_t batch) {
    Frontier frontier(batch);
    Golden golden;
    for (int i = 0; i < 4096; ++i) {
        const Key key{i % 23, (i / 3) % 19, i % 7};
        append(frontier, golden, key, (i % 113) - 71, 53 - (i % 131), batch);
    }
    append(frontier, golden, {3, 5, 1}, 1000, -1000, batch);
    append(frontier, golden, {3, 5, 1}, -1000, 2000, batch);
    check_take(frontier, golden);
    require(frontier.compactions() > 1, "duplicate fan-in must exercise repeated compactions");

    Golden reused;
    append(frontier, reused, {9, 8, 7}, -9, -8, batch);
    check_take(frontier, reused);
}

void randomized_unique_and_duplicate_order(std::size_t batch) {
    Frontier frontier(batch);
    Golden golden;
    std::vector<Key> keys;
    for (int i = 0; i < 1024; ++i) keys.push_back({i, i % 31, -i});
    std::uint32_t random = 0x12345678U;
    for (std::size_t i = keys.size(); i > 1; --i) {
        std::swap(keys[i - 1], keys[next_random(random) % i]);
    }
    for (const auto& key : keys) {
        const auto value = static_cast<std::int64_t>(next_random(random) % 2001) - 1000;
        append(frontier, golden, key, value, -value - 17, batch);
    }
    for (std::size_t i = keys.size(); i > 1; --i) {
        std::swap(keys[i - 1], keys[next_random(random) % i]);
    }
    for (const auto& key : keys) {
        const auto value = static_cast<std::int64_t>(next_random(random) % 2001) - 1000;
        append(frontier, golden, key, value, value * 2 - 3, batch);
    }
    check_take(frontier, golden);
}

}  // namespace

int main() {
    try {
        constexpr std::array<std::size_t, 4> batches{1, 2, 7, 64};
        for (const std::size_t batch : batches) {
            duplicate_fanin(batch);
            randomized_unique_and_duplicate_order(batch);
        }

        Frontier empty(64);
        require(empty.take_sorted().empty() && empty.pending_cells() == 0,
                "empty take must be stable");
        std::cout << "compact score frontier checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
