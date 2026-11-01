#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace pancollapse {

// Exact max-plus frontier for states without variable support payloads. Pending
// duplicates can be merged at any time: each objective has its own maximum.
// batch is an allocation/compaction trigger, never a limit on retained states.
template<class Key>
class CompactScoreFrontier {
  public:
    struct Cell {
        Key key;
        std::int64_t score = 0, gene_score = 0;
    };

    explicit CompactScoreFrontier(std::size_t batch = 64)
        : batch_(std::max<std::size_t>(1, batch)), next_compaction_(batch_) {}

    void add(const Key& key, std::int64_t score, std::int64_t gene_score) {
        // Request the next compaction threshold rather than implicit geometric
        // growth. Allocators may reserve more; pending cell count is bounded.
        if (cells_.size() == cells_.capacity()) cells_.reserve(next_compaction_);
        cells_.push_back({key, score, gene_score});
        if (cells_.size() >= next_compaction_) compact();
    }

    std::vector<Cell> take_sorted() {
        compact();
        auto result = std::move(cells_);
        cells_.clear();
        next_compaction_ = batch_;
        return result;
    }

    std::size_t pending_cells() const noexcept { return cells_.size(); }
    std::size_t capacity() const noexcept { return cells_.capacity(); }
    std::uint64_t compactions() const noexcept { return compactions_; }

  private:
    void compact() {
        if (cells_.empty()) return;
        std::sort(cells_.begin(), cells_.end(),
            [](const auto& a, const auto& b) { return a.key < b.key; });
        std::size_t retained = 0;
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            if (retained != 0 && cells_[retained - 1].key == cells_[i].key) {
                auto& into = cells_[retained - 1];
                into.score = std::max(into.score, cells_[i].score);
                into.gene_score = std::max(into.gene_score, cells_[i].gene_score);
            } else {
                if (retained != i) cells_[retained] = std::move(cells_[i]);
                ++retained;
            }
        }
        cells_.resize(retained);
        const auto growth = std::max(retained, batch_);
        next_compaction_ = growth > std::numeric_limits<std::size_t>::max() - retained
            ? std::numeric_limits<std::size_t>::max() : retained + growth;
        ++compactions_;
    }

    std::vector<Cell> cells_;
    std::size_t batch_ = 64, next_compaction_ = 64;
    std::uint64_t compactions_ = 0;
};

}  // namespace pancollapse
