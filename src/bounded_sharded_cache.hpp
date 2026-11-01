#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace pancollapse {

// A bounded insertion-only cache. Builders run without a shard lock: two racing
// builders may both run, but the first inserted exact key wins and the other is
// counted as a hit. Retention can be disabled permanently under memory pressure;
// leases already returned to callers remain valid. Per-shard entry bounds preserve
// the legacy cache behavior; an optional global byte bound limits approximate
// retained entry storage (key heap, map/control overhead, and a caller-supplied
// value estimate).
template<class Key, class Value, std::size_t ShardCount = 256, class Hash = std::hash<Key>>
class BoundedShardedCache {
  public:
    struct ClearResult {
        std::size_t entries = 0;
        std::size_t charged_bytes = 0;
    };

    explicit BoundedShardedCache(
        std::size_t maximum_entries,
        std::size_t maximum_bytes = std::numeric_limits<std::size_t>::max(),
        std::function<std::size_t(const Value&)> value_bytes = {})
        : entries_per_shard_(maximum_entries / ShardCount +
                             static_cast<std::size_t>(maximum_entries % ShardCount != 0)),
          maximum_bytes_(maximum_bytes), value_bytes_(std::move(value_bytes)) {
        static_assert(ShardCount > 0);
        if (maximum_entries == 0) {
            throw std::invalid_argument("bounded cache capacity must be positive");
        }
    }

    template<class Builder>
    std::shared_ptr<const Value> get_or_build(const Key& key, Builder&& builder) {
        Shard& shard = shards_[Hash{}(key) % ShardCount];
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            const auto found = shard.values.find(key);
            if (found != shard.values.end()) {
                hits_.fetch_add(1, std::memory_order_relaxed);
                return found->second;
            }
        }

        auto value = std::make_shared<const Value>(builder());
        bool charge_overflow = false;
        const std::size_t bytes = entry_charge(key, *value, charge_overflow);

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        const auto found = shard.values.find(key);
        if (found != shard.values.end()) {
            // Another worker inserted this key while ours was building. Report it as a hit so the
            // pre-retirement counters stay those of single-flight construction.
            hits_.fetch_add(1, std::memory_order_relaxed);
            return found->second;
        }

        misses_.fetch_add(1, std::memory_order_relaxed);
        // Disabling retention precedes taking every shard exclusively. An
        // already-publishing builder is subsequently cleared; a builder that
        // arrives later observes this check and cannot repopulate the cache.
        if (!retention_enabled_.load(std::memory_order_acquire) || charge_overflow ||
            shard.values.size() >= entries_per_shard_ || !reserve_bytes(bytes)) {
            uncached_.fetch_add(1, std::memory_order_relaxed);
            return value;
        }
        try {
            shard.values.emplace(key, value);
        } catch (...) {
            release_bytes(bytes);
            throw;
        }
        entries_.fetch_add(1, std::memory_order_relaxed);
        return value;
    }

    ClearResult disable_retention_and_clear() {
        retention_enabled_.store(false, std::memory_order_release);
        for (Shard& shard : shards_) {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            ValueMap empty;
            shard.values.swap(empty);
        }
        return {entries_.exchange(0, std::memory_order_relaxed),
                charged_bytes_.exchange(0, std::memory_order_relaxed)};
    }

    // Heap held by the cache: map nodes, buckets, keys, and whatever `value_bytes` reports for a
    // value, excluding allocator overhead. Call only after the workers have stopped.
    template<class ValueBytes>
    std::size_t approximate_bytes(ValueBytes&& value_bytes) const {
        std::size_t total = 0;
        for (const Shard& shard : shards_) {
            total += shard.values.bucket_count() * sizeof(void*);
            for (const auto& [key, value] : shard.values) {
                total += sizeof(typename decltype(shard.values)::value_type) + 2 * sizeof(void*);
                total += string_heap_bytes(key);
                // make_shared: one block holding the control block and the value.
                total += 2 * sizeof(long) + value_bytes(*value);
            }
        }
        return total;
    }

    static std::size_t string_heap_bytes(const std::string& value) {
        return value.capacity() > 15 ? value.capacity() + 1 : 0;
    }

    std::size_t hits() const { return hits_.load(std::memory_order_relaxed); }
    std::size_t misses() const { return misses_.load(std::memory_order_relaxed); }
    std::size_t entries() const { return entries_.load(std::memory_order_relaxed); }
    std::size_t uncached() const { return uncached_.load(std::memory_order_relaxed); }
    std::size_t charged_bytes() const { return charged_bytes_.load(std::memory_order_relaxed); }
    bool retention_enabled() const {
        return retention_enabled_.load(std::memory_order_acquire);
    }

  private:
    using ValueMap = std::unordered_map<Key, std::shared_ptr<const Value>, Hash>;

    struct Shard {
        std::shared_mutex mutex;
        ValueMap values;
    };

    bool reserve_bytes(std::size_t bytes) {
        std::size_t charged = charged_bytes_.load(std::memory_order_relaxed);
        for (;;) {
            // Check arithmetic overflow before testing the configured cap. This also keeps the
            // default "unbounded" setting from wrapping the diagnostic counter.
            if (bytes > std::numeric_limits<std::size_t>::max() - charged ||
                bytes > maximum_bytes_ || charged > maximum_bytes_ - bytes) {
                return false;
            }
            if (charged_bytes_.compare_exchange_weak(charged, charged + bytes,
                                                     std::memory_order_relaxed,
                                                     std::memory_order_relaxed)) {
                return true;
            }
        }
    }

    void release_bytes(std::size_t bytes) {
        if (bytes != 0) charged_bytes_.fetch_sub(bytes, std::memory_order_relaxed);
    }

    static bool checked_add(std::size_t& total, std::size_t value) {
        if (value > std::numeric_limits<std::size_t>::max() - total) return false;
        total += value;
        return true;
    }

    std::size_t entry_charge(const Key& key, const Value& value, bool& overflow) const {
        if (!value_bytes_) return 0;
        std::size_t result = 0;
        overflow = !checked_add(result, value_bytes_(value));
        if constexpr (std::is_same_v<std::remove_cv_t<Key>, std::string>) {
            overflow = overflow || !checked_add(result, string_heap_bytes(key));
        }
        // Match approximate_bytes()'s map/control accounting, plus one pointer
        // of bucket capacity per retained entry as a stable amortized estimate.
        overflow = overflow ||
            !checked_add(result, sizeof(typename ValueMap::value_type) + 2 * sizeof(void*)) ||
            !checked_add(result, 2 * sizeof(long)) ||
            !checked_add(result, sizeof(void*));
        return result;
    }

    std::size_t entries_per_shard_;
    std::size_t maximum_bytes_;
    std::function<std::size_t(const Value&)> value_bytes_;
    std::array<Shard, ShardCount> shards_;
    std::atomic<std::size_t> hits_{0};
    std::atomic<std::size_t> misses_{0};
    std::atomic<std::size_t> entries_{0};
    std::atomic<std::size_t> uncached_{0};
    std::atomic<std::size_t> charged_bytes_{0};
    std::atomic<bool> retention_enabled_{true};
};

}  // namespace pancollapse
