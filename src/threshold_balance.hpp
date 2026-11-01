#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace pancollapse::threshold_balance {

struct Delta {
    std::int64_t exon = 0, strand = 0, score = 0, gene_score = 0;
};

// Unsafe inputs disable the optimization; the caller's original checked
// arithmetic still reports its original error on the unchanged execution path.
inline std::optional<Delta> make_delta(std::uint64_t exon, std::uint64_t bases,
                                       std::uint64_t forward, std::uint64_t reverse,
                                       std::int64_t score, std::int64_t gene_score) {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if (exon > maximum || bases > maximum || forward > maximum || reverse > maximum) return {};
    const __int128 margin = 2 * static_cast<__int128>(exon) - bases;
    if (margin < std::numeric_limits<std::int64_t>::min() || margin > maximum) return {};
    return Delta{static_cast<std::int64_t>(margin),
                 static_cast<std::int64_t>(forward) - static_cast<std::int64_t>(reverse),
                 score, gene_score};
}

struct Range {
    std::int64_t low = 0, high = 0;

    std::int64_t normalize(std::int64_t balance) const noexcept {
        if (balance >= -low) return -low;
        if (balance < -high) return -high - 1;
        return balance;
    }
};
struct Bounds { Range exon, strand; };

// Bounds include the current piece and every possible complete suffix. EachNext
// invokes its visitor for both normal and connection edges. An empty result
// means the conservative overflow proof failed, not that any route is discarded.
template<class EachNext>
std::vector<Bounds> prepare(std::span<const Delta> deltas, EachNext&& each_next) {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto balance_limit = (maximum - 1) / 2;
    __int128 exon_sum = 0, strand_sum = 0, score_sum = 0, gene_sum = 0;
    const auto magnitude = [](std::int64_t value) -> __int128 {
        return value < 0 ? -static_cast<__int128>(value) : value;
    };
    for (const auto& delta : deltas) {
        exon_sum += magnitude(delta.exon);
        strand_sum += magnitude(delta.strand);
        score_sum += magnitude(delta.score);
        gene_sum += magnitude(delta.gene_score);
        // A real prefix/suffix has magnitude <= S. A representative has
        // magnitude <= S+1, hence its next addition is bounded by 2*S+1.
        if (exon_sum > balance_limit || strand_sum > balance_limit ||
            score_sum > maximum || gene_sum > maximum) return {};
    }
    std::vector<Bounds> result(deltas.size());
    for (std::size_t i = deltas.size(); i-- > 0;) {
        Bounds suffix;
        bool first = true;
        each_next(i, [&](std::size_t next) {
            if (next <= i || next >= deltas.size())
                throw std::logic_error("threshold-balance DAG is not topologically ordered");
            const auto& child = result[next];
            if (first) { suffix = child; first = false; }
            else {
                suffix.exon.low = std::min(suffix.exon.low, child.exon.low);
                suffix.exon.high = std::max(suffix.exon.high, child.exon.high);
                suffix.strand.low = std::min(suffix.strand.low, child.strand.low);
                suffix.strand.high = std::max(suffix.strand.high, child.strand.high);
            }
        });
        result[i] = {{deltas[i].exon + suffix.exon.low, deltas[i].exon + suffix.exon.high},
                     {deltas[i].strand + suffix.strand.low, deltas[i].strand + suffix.strand.high}};
    }
    return result;
}

}  // namespace pancollapse::threshold_balance
