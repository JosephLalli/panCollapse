#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pancollapse {

// Scratch space for one alignment. Hash/equality must ignore score. Preserve the
// first occurrence of each identity, replacing only its score with the maximum.
// Slots refer into the compacted prefix, so there are no per-state allocations.
class StableScoreDedup {
public:
    template<class State, class Hash, class Equal>
    void retain_best(std::vector<State>& states, Hash hash, Equal equal) {
        if (states.size() < 2) {
            return;
        }
        constexpr auto empty = std::numeric_limits<std::size_t>::max();
        constexpr auto highest_power = std::size_t{1} <<
            (std::numeric_limits<std::size_t>::digits - 1);
        if (states.size() > highest_power / 2) {
            throw std::length_error("too many exact evidence states");
        }
        const auto buckets = std::bit_ceil(states.size() * 2);
        slots_.resize(buckets);
        std::fill(slots_.begin(), slots_.end(), empty);
        const auto mask = buckets - 1;
        std::size_t retained = 0;
        for (std::size_t index = 0; index < states.size(); ++index) {
            auto slot = hash(states[index]) & mask;
            while (slots_[slot] != empty &&
                   !equal(states[slots_[slot]], states[index])) {
                slot = (slot + 1) & mask;
            }
            if (slots_[slot] == empty) {
                slots_[slot] = retained;
                if (retained != index) {
                    states[retained] = std::move(states[index]);
                }
                ++retained;
            } else {
                auto& best = states[slots_[slot]];
                best.score = std::max(best.score, states[index].score);
            }
        }
        states.resize(retained);
    }

private:
    std::vector<std::size_t> slots_;
};

// Final evidence order is comparator order, unlike DP state first-occurrence
// order. Identity-equivalent evidence carries the same non-score metadata.
template<class Evidence, class Less>
void sort_best_evidence(std::vector<Evidence>& evidence, Less less) {
    std::sort(evidence.begin(), evidence.end(), less);
    std::size_t retained = 0;
    for (std::size_t index = 0; index < evidence.size(); ++index) {
        if (retained && !less(evidence[retained - 1], evidence[index])) {
            evidence[retained - 1].score =
                std::max(evidence[retained - 1].score, evidence[index].score);
        } else {
            if (retained != index) {
                evidence[retained] = std::move(evidence[index]);
            }
            ++retained;
        }
    }
    evidence.resize(retained);
}

} // namespace pancollapse
