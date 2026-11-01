#include "threshold_balance.hpp"

#include <array>
#include <functional>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using namespace pancollapse::threshold_balance;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<Bounds> plan(const std::vector<Delta>& deltas,
                         const std::vector<std::vector<std::size_t>>& next) {
    return prepare(deltas, [&](std::size_t i, const auto& visit) {
        for (const auto child : next[i]) visit(child);
    });
}
}  // namespace

int main() {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto limit = (maximum - 1) / 2;
    const Range range{-4, 7};
    for (const auto [input, expected] : std::array<std::array<std::int64_t, 2>, 6>{
             {{4, 4}, {3, 3}, {-7, -7}, {-8, -8}, {-100, -8}, {100, 4}}})
        require(range.normalize(input) == expected, "inclusive threshold boundary");

    require(make_delta(5, 10, 10, 0, 7, -3)->exon == 0, "exact fifty percent margin");
    require(!make_delta(static_cast<std::uint64_t>(maximum) + 1, 0, 0, 0, 0, 0),
            "invalid signed exon bases must retain checked fallback");
    require(!make_delta(0, static_cast<std::uint64_t>(maximum) + 1, 0, 0, 0, 0),
            "invalid signed total bases must retain checked fallback");
    require(!make_delta(0, 0, static_cast<std::uint64_t>(maximum) + 1, 0, 0, 0),
            "invalid forward bases must retain checked fallback");
    require(!make_delta(0, 0, 0, static_cast<std::uint64_t>(maximum) + 1, 0, 0),
            "invalid reverse bases must retain checked fallback");
    require(!make_delta(maximum, 0, 0, 0, 0, 0), "margin overflow must retain checked fallback");
    require(plan({{limit, -limit, maximum, -maximum}}, {{}}).size() == 1,
            "safe conservative guard boundary");
    require(plan({{limit + 1, 0, 0, 0}}, {{}}).empty(), "canonical intermediate overflow guard");
    require(plan({{0, -limit - 1, 0, 0}}, {{}}).empty(), "negative balance overflow guard");
    require(plan({{0, 0, minimum, 0}}, {{}}).empty(), "INT64_MIN score magnitude");
    require(plan({{0, 0, 0, minimum}}, {{}}).empty(), "INT64_MIN gene magnitude");
    require(plan({{0, 0, maximum, 0}, {0, 0, 1, 0}}, {{1}, {}}).empty(),
            "score-prefix overflow guard");
    require(plan({{0, 0, 0, maximum}, {0, 0, 0, -1}}, {{}, {}}).empty(),
            "unreachable pieces conservatively participate in proof");
    try {
        (void)plan({{}}, {{0}});
        throw std::runtime_error("invalid topology accepted");
    } catch (const std::logic_error&) {}

    std::mt19937 random(0x5317);
    for (int trial = 0; trial < 128; ++trial) {
        const std::size_t size = 1 + random() % 7;
        std::vector<Delta> deltas(size);
        std::vector<std::vector<std::size_t>> next(size);
        for (std::size_t i = 0; i < size; ++i) {
            deltas[i] = {static_cast<std::int64_t>(random() % 11) - 5,
                         static_cast<std::int64_t>(random() % 11) - 5,
                         static_cast<std::int64_t>(random() % 19) - 9,
                         static_cast<std::int64_t>(random() % 19) - 9};
            for (std::size_t j = i + 1; j < size; ++j)
                if (random() % 3 == 0) next[i].push_back(j);
        }
        const auto bounds = plan(deltas, next);
        require(bounds.size() == size, "small DAG must pass safety proof");
        // Treat every vertex as a possible start. This independently enumerates
        // complete suffixes and checks repeated normalization at every merge.
        for (std::size_t start = 0; start < size; ++start)
            for (std::int64_t initial = -20; initial <= 20; ++initial) {
                std::function<void(std::size_t, std::int64_t, std::int64_t,
                                   std::int64_t, std::int64_t)> walk;
                walk = [&](std::size_t i, std::int64_t exon, std::int64_t strand,
                           std::int64_t canonical_exon, std::int64_t canonical_strand) {
                    exon += deltas[i].exon;
                    strand += deltas[i].strand;
                    canonical_exon += deltas[i].exon;
                    canonical_strand += deltas[i].strand;
                    if (next[i].empty()) {
                        require((exon >= 0) == (canonical_exon >= 0), "suffix exon sign changed");
                        require((strand >= 0) == (canonical_strand >= 0), "suffix strand sign changed");
                    }
                    for (const auto child : next[i])
                        walk(child, exon, strand, bounds[child].exon.normalize(canonical_exon),
                             bounds[child].strand.normalize(canonical_strand));
                };
                walk(start, initial, -initial, bounds[start].exon.normalize(initial),
                     bounds[start].strand.normalize(-initial));
            }
    }
}
