#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace pancollapse {

// A byte-accounted immutable-value cache. Construction is single-flight per key;
// different keys, including keys in the same shard, build concurrently. FIFO
// eviction changes only reuse: returned shared_ptr leases keep values valid after
// eviction. The resident byte cap excludes allocator-specific overhead, bounded
// in-flight control state, and values retained solely by external leases.
template<class Key, class Value, std::size_t ShardCount = 256,
         class Hash = std::hash<Key>>
class BoundedReferenceCache {
  public:
    using value_ptr = std::shared_ptr<const Value>;
    using value_size_function = std::function<std::size_t(const Value&)>;

    BoundedReferenceCache(std::size_t maximum_resident_bytes,
                          value_size_function value_size)
        : maximum_resident_bytes_(maximum_resident_bytes),
          value_size_(std::move(value_size)) {
        static_assert(ShardCount > 0);
        if (maximum_resident_bytes == 0) {
            throw std::invalid_argument("bounded reference cache capacity must be positive");
        }
        if (!value_size_) {
            throw std::invalid_argument("bounded reference cache needs a value-size function");
        }
    }

    // Startup only: the caller owns the cache exclusively and has not issued a
    // query. Holding every shard also detects builders whose value is not ready.
    void configure_capacity(std::size_t maximum_resident_bytes) {
        if (maximum_resident_bytes == 0) {
            throw std::invalid_argument("bounded reference cache capacity must be positive");
        }
        std::array<std::unique_lock<std::shared_mutex>, ShardCount> locks;
        for (std::size_t i = 0; i < ShardCount; ++i) {
            locks[i] = std::unique_lock<std::shared_mutex>(shards_[i].mutex);
        }
        for (const Shard& shard : shards_) {
            if (!shard.values.empty() || !shard.flights.empty()) {
                throw std::logic_error("cache capacity must be configured before queries");
            }
        }
        if (build_count() != 0 || hits() != 0) {
            throw std::logic_error("cache capacity must be configured before queries");
        }
        maximum_resident_bytes_ = maximum_resident_bytes;
    }

    template<class Builder>
    value_ptr get_or_build(const Key& key, Builder&& builder) {
        const std::size_t shard_number = Hash{}(key) % ShardCount;
        Shard& shard = shards_[shard_number];
        std::shared_ptr<Flight> flight;
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            const auto found = shard.values.find(key);
            if (found != shard.values.end()) {
                hits_.fetch_add(1, std::memory_order_relaxed);
                return found->second.value;
            }
            const auto pending = shard.flights.find(key);
            if (pending != shard.flights.end()) {
                flight = pending->second;
                // A flight join is a hit even if its owner later fails: this
                // request did not become an owner or duplicate the build.
                hits_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        if (flight) return wait_for_flight(flight);

        bool owner = false;
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            const auto found = shard.values.find(key);
            if (found != shard.values.end()) {
                hits_.fetch_add(1, std::memory_order_relaxed);
                return found->second.value;
            }
            const auto pending = shard.flights.find(key);
            if (pending != shard.flights.end()) {
                flight = pending->second;
                hits_.fetch_add(1, std::memory_order_relaxed);
            } else {
                flight = std::make_shared<Flight>();
                const auto [inserted, fresh] = shard.flights.emplace(key, flight);
                if (!fresh) {
                    flight = inserted->second;
                    hits_.fetch_add(1, std::memory_order_relaxed);
                } else {
                    owner = true;
                }
            }
        }
        if (!owner) return wait_for_flight(flight);

        value_ptr value;
        std::size_t charge = 0;
        try {
            // Neither construction nor sizing holds a shard lock. Preserve the
            // original diagnostic order: a constructed value counts as a build
            // even if value_size subsequently throws.
            value = std::make_shared<const Value>(builder());
            build_count_.fetch_add(1, std::memory_order_relaxed);
            charge = entry_bytes(*value);
            update_maximum(maximum_entry_bytes_, charge);
        } catch (...) {
            fail_flight(shard, key, flight, std::current_exception());
            throw;
        }

        try {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            const auto pending = shard.flights.find(key);
            if (pending == shard.flights.end() || pending->second != flight) {
                throw std::logic_error("bounded reference cache flight is inconsistent");
            }
            const std::size_t budget = shard_budget(shard_number);
            if (charge > budget) {
                shard.flights.erase(pending);
                uncached_.fetch_add(1, std::memory_order_relaxed);
            } else {
                while (!shard.fifo.empty() && charge > budget - shard.resident_bytes) {
                    Key evicted_key = std::move(shard.fifo.front());
                    shard.fifo.pop_front();
                    const auto evicted = shard.values.find(evicted_key);
                    if (evicted == shard.values.end()) {
                        throw std::logic_error("bounded reference cache FIFO is inconsistent");
                    }
                    const std::size_t released = evicted->second.bytes;
                    shard.resident_bytes -= released;
                    shard.values.erase(evicted);
                    resident_bytes_.fetch_sub(released, std::memory_order_relaxed);
                    entries_.fetch_sub(1, std::memory_order_relaxed);
                    evictions_.fetch_add(1, std::memory_order_relaxed);
                }

                shard.fifo.push_back(key);
                try {
                    const auto [inserted, fresh] = shard.values.emplace(key, Entry{value, charge});
                    static_cast<void>(inserted);
                    if (!fresh) {
                        throw std::logic_error("bounded reference cache value is inconsistent");
                    }
                } catch (...) {
                    shard.fifo.pop_back();
                    throw;
                }
                shard.flights.erase(pending);
                shard.resident_bytes += charge;
                entries_.fetch_add(1, std::memory_order_relaxed);
                const std::size_t resident =
                    resident_bytes_.fetch_add(charge, std::memory_order_relaxed) + charge;
                update_high_water(resident);
            }
        } catch (...) {
            fail_flight(shard, key, flight, std::current_exception());
            throw;
        }

        complete_flight(flight, value);
        return value;
    }

    // Requires all workers to have joined, so no flight may be active. Cumulative
    // diagnostics and high-water stay intact and the cache remains reusable.
    void clear() {
        for (Shard& shard : shards_) {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            if (!shard.flights.empty()) {
                throw std::logic_error("bounded reference cache clear requires joined workers");
            }
            const std::size_t released = shard.resident_bytes;
            const std::size_t removed = shard.values.size();
            std::unordered_map<Key, Entry, Hash>().swap(shard.values);
            std::unordered_map<Key, std::shared_ptr<Flight>, Hash>().swap(shard.flights);
            std::deque<Key>().swap(shard.fifo);
            shard.resident_bytes = 0;
            resident_bytes_.fetch_sub(released, std::memory_order_relaxed);
            entries_.fetch_sub(removed, std::memory_order_relaxed);
        }
    }

    std::size_t maximum_resident_bytes() const { return maximum_resident_bytes_; }
    std::size_t resident_bytes() const {
        return resident_bytes_.load(std::memory_order_relaxed);
    }
    std::size_t high_water_bytes() const {
        return high_water_bytes_.load(std::memory_order_relaxed);
    }
    std::size_t maximum_entry_bytes() const {
        return maximum_entry_bytes_.load(std::memory_order_relaxed);
    }
    std::size_t entries() const { return entries_.load(std::memory_order_relaxed); }
    std::size_t build_count() const {
        return build_count_.load(std::memory_order_relaxed);
    }
    // Ready-value returns plus flight joins (including joins to failed flights).
    std::size_t hits() const { return hits_.load(std::memory_order_relaxed); }
    std::size_t evictions() const { return evictions_.load(std::memory_order_relaxed); }
    std::size_t uncached() const { return uncached_.load(std::memory_order_relaxed); }

  private:
    // Covers the map node, a conservative share of bucket/control-block storage,
    // and the caller-supplied Value object plus owned buffers.
    static constexpr std::size_t entry_overhead =
        sizeof(std::pair<const Key, value_ptr>) + sizeof(Key) +
        4 * sizeof(void*) + 2 * sizeof(long);

    struct Entry {
        value_ptr value;
        std::size_t bytes = 0;
    };
    struct Flight {
        std::mutex mutex;
        std::condition_variable condition;
        value_ptr value;
        std::exception_ptr error;
        bool complete = false;
    };
    struct Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<Key, Entry, Hash> values;
        std::unordered_map<Key, std::shared_ptr<Flight>, Hash> flights;
        std::deque<Key> fifo;
        std::size_t resident_bytes = 0;
    };

    static value_ptr wait_for_flight(const std::shared_ptr<Flight>& flight) {
        std::unique_lock<std::mutex> lock(flight->mutex);
        flight->condition.wait(lock, [&] { return flight->complete; });
        if (flight->error) std::rethrow_exception(flight->error);
        if (!flight->value) {
            throw std::logic_error("bounded reference cache flight completed without a value");
        }
        return flight->value;
    }

    static void complete_flight(const std::shared_ptr<Flight>& flight, const value_ptr& value) {
        {
            std::lock_guard<std::mutex> lock(flight->mutex);
            flight->value = value;
            flight->complete = true;
        }
        flight->condition.notify_all();
    }

    static void complete_flight(const std::shared_ptr<Flight>& flight,
                                const std::exception_ptr& error) {
        {
            std::lock_guard<std::mutex> lock(flight->mutex);
            flight->error = error;
            flight->complete = true;
        }
        flight->condition.notify_all();
    }

    static void fail_flight(Shard& shard, const Key& key,
                            const std::shared_ptr<Flight>& flight,
                            const std::exception_ptr& error) {
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            const auto pending = shard.flights.find(key);
            if (pending != shard.flights.end() && pending->second == flight) {
                shard.flights.erase(pending);
            }
        }
        complete_flight(flight, error);
    }

    std::size_t shard_budget(std::size_t shard_number) const {
        return maximum_resident_bytes_ / ShardCount +
               (shard_number < maximum_resident_bytes_ % ShardCount);
    }

    std::size_t entry_bytes(const Value& value) const {
        const std::size_t value_bytes = value_size_(value);
        if (value_bytes > std::numeric_limits<std::size_t>::max() - entry_overhead) {
            return std::numeric_limits<std::size_t>::max();
        }
        return entry_overhead + value_bytes;
    }

    static void update_maximum(std::atomic<std::size_t>& counter, std::size_t value) {
        std::size_t previous = counter.load(std::memory_order_relaxed);
        while (previous < value &&
               !counter.compare_exchange_weak(previous, value, std::memory_order_relaxed)) {
        }
    }

    void update_high_water(std::size_t resident) {
        update_maximum(high_water_bytes_, resident);
    }

    std::size_t maximum_resident_bytes_;
    value_size_function value_size_;
    std::array<Shard, ShardCount> shards_;
    std::atomic<std::size_t> resident_bytes_{0};
    std::atomic<std::size_t> high_water_bytes_{0};
    std::atomic<std::size_t> maximum_entry_bytes_{0};
    std::atomic<std::size_t> entries_{0};
    std::atomic<std::size_t> build_count_{0};
    std::atomic<std::size_t> hits_{0};
    std::atomic<std::size_t> evictions_{0};
    std::atomic<std::size_t> uncached_{0};
};

}  // namespace pancollapse
