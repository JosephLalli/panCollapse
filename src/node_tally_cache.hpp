#pragma once

#include "pathtally.hpp"

#include <memory>

namespace pathtally {

// Worker-owned and cleared between read groups. Each resolved entry leases its
// immutable node metadata even if the shared cache evicts that key concurrently.
template<class Metadata>
class ReadNodeTallyCache {
public:
    void clear() {
        nodes_.clear();
        first_seen_.clear();
    }

    bool add(int64_t node_id, bool reverse, int64_t score, int64_t bases) {
        Entry& entry = get(node_id);
        int64_t next_score, next_bases;
        int64_t& oriented = reverse ? entry.totals.reverse_bases : entry.totals.forward_bases;
        if (__builtin_add_overflow(entry.totals.score, score, &next_score) ||
            __builtin_add_overflow(oriented, bases, &next_bases)) {
            return false;
        }
        entry.totals.score = next_score;
        oriented = next_bases;
        return true;
    }

    template<class Lookup>
    const Metadata& resolve(int64_t node_id, const Lookup& lookup) {
        Entry& entry = get(node_id);
        if (!entry.metadata) entry.metadata = lookup(node_id);
        return *entry.metadata;
    }

    // False requests replay in the original mapping order: regrouping must not
    // introduce signed overflow even when the original sequence of additions fits.
    template<class Lookup>
    bool expand(NumericTallyMap& tallies, const Lookup& lookup) {
        tallies.clear();
        for (const int64_t node_id : first_seen_) {
            Entry& entry = nodes_.at(node_id);
            if (!entry.metadata) entry.metadata = lookup(node_id);
            const HstTally& total = entry.totals;
            for (const auto& reference : entry.metadata->ref_paths) {
                HstTally& tally = tallies[reference.path_handle];
                const int64_t forward = reference.path_is_reverse
                    ? total.reverse_bases : total.forward_bases;
                const int64_t reverse = reference.path_is_reverse
                    ? total.forward_bases : total.reverse_bases;
                int64_t score_sum, forward_sum, reverse_sum;
                if (__builtin_add_overflow(tally.score, total.score, &score_sum) ||
                    __builtin_add_overflow(tally.forward_bases, forward, &forward_sum) ||
                    __builtin_add_overflow(tally.reverse_bases, reverse, &reverse_sum)) {
                    return false;
                }
                tally = {score_sum, forward_sum, reverse_sum};
            }
        }
        return true;
    }

private:
    struct Entry {
        HstTally totals;
        // The shared cache may evict this value while the read is still using
        // it. Retain one worker-local lease until clear() starts the next read.
        std::shared_ptr<const Metadata> metadata;
    };

    Entry& get(int64_t node_id) {
        auto [found, inserted] = nodes_.try_emplace(node_id);
        if (inserted) first_seen_.push_back(node_id);
        return found->second;
    }

    absl::flat_hash_map<int64_t, Entry> nodes_;
    std::vector<int64_t> first_seen_;
};

}  // namespace pathtally
