#include "bounded_reference_cache.hpp"
#include "bounded_sharded_cache.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

std::size_t string_bytes(const std::string& value) {
    return sizeof(value) + value.capacity() + 1;
}

constexpr auto wait_timeout = std::chrono::seconds(30);

class BuilderGate {
  public:
    void enter_and_wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        ++entered_;
        condition_.notify_all();
        condition_.wait(lock, [&] { return released_; });
    }

    bool wait_for_entered(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, wait_timeout, [&] { return entered_ >= count; });
    }

    void release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            released_ = true;
        }
        condition_.notify_all();
    }

  private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::size_t entered_ = 0;
    bool released_ = false;
};

template<class Predicate>
bool wait_until(Predicate&& predicate) {
    const auto deadline = std::chrono::steady_clock::now() + wait_timeout;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

struct ThrowingKey {
    int value = 0;
    std::shared_ptr<std::atomic<bool>> throw_on_copy;

    ThrowingKey() = default;
    ThrowingKey(int value, std::shared_ptr<std::atomic<bool>> throw_on_copy)
        : value(value), throw_on_copy(std::move(throw_on_copy)) {}
    ThrowingKey(const ThrowingKey& other)
        : value(other.value), throw_on_copy(other.throw_on_copy) {
        if (throw_on_copy && throw_on_copy->load(std::memory_order_relaxed)) {
            throw std::runtime_error("expected publication failure");
        }
    }
    ThrowingKey(ThrowingKey&&) noexcept = default;
    ThrowingKey& operator=(const ThrowingKey&) = default;
    ThrowingKey& operator=(ThrowingKey&&) noexcept = default;

    bool operator==(const ThrowingKey& other) const noexcept { return value == other.value; }
};

struct ThrowingKeyHash {
    std::size_t operator()(const ThrowingKey& key) const noexcept {
        return std::hash<int>{}(key.value);
    }
};

}  // namespace

int main() {
    try {
        using Cache = pancollapse::BoundedReferenceCache<int, std::string, 1>;
        Cache configured(1, string_bytes);
        configured.configure_capacity(4096);
        check(configured.maximum_resident_bytes() == 4096,
              "startup cache configuration did not update the byte limit");
        auto configured_value = configured.get_or_build(1, [] { return std::string(32, 'c'); });
        bool rejected_used_cache = false;
        try { configured.configure_capacity(8192); }
        catch (const std::logic_error&) { rejected_used_cache = true; }
        check(rejected_used_cache && configured.maximum_resident_bytes() == 4096 &&
                  configured_value->size() == 32,
              "used cache configuration must reject without changing its limit or leases");

        Cache configuring_flight(4096, string_bytes);
        BuilderGate configuration_gate;
        auto pending_configuration = std::async(std::launch::async, [&] {
            return configuring_flight.get_or_build(1, [&] {
                configuration_gate.enter_and_wait();
                return std::string(32, 'f');
            });
        });
        const bool configuration_entered = configuration_gate.wait_for_entered(1);
        bool rejected_flight = false;
        if (configuration_entered) {
            try { configuring_flight.configure_capacity(8192); }
            catch (const std::logic_error&) { rejected_flight = true; }
        }
        configuration_gate.release();
        const auto configured_flight_value = pending_configuration.get();
        check(configuration_entered && rejected_flight && configured_flight_value->size() == 32 &&
                  configuring_flight.maximum_resident_bytes() == 4096,
              "startup reconfiguration must detect an unfinished cache builder");

        Cache cache(1024, string_bytes);

        auto first = cache.get_or_build(1, [] { return std::string(800, 'a'); });
        check(cache.entries() == 1 && cache.resident_bytes() <= 1024,
              "first value was not cached within the byte bound");
        check(cache.maximum_entry_bytes() == cache.resident_bytes(),
              "single-entry maximum charge differs from residency");
        const std::size_t first_high_water = cache.high_water_bytes();
        auto second = cache.get_or_build(2, [] { return std::string(800, 'b'); });
        check(cache.entries() == 1 && cache.evictions() == 1,
              "second value did not evict the FIFO entry");
        check(cache.resident_bytes() <= cache.maximum_resident_bytes(),
              "resident byte bound exceeded after eviction");
        check(first->size() == 800 && first->front() == 'a',
              "eviction invalidated an active lease");
        check(second->size() == 800 && second->front() == 'b',
              "second cached value changed");

        auto first_again = cache.get_or_build(1, [] { return std::string(800, 'a'); });
        check(*first_again == *first && first_again.get() != first.get(),
              "evicted key was not rebuilt equivalently");
        check(cache.build_count() == 3 && cache.evictions() == 2,
              "rebuild diagnostics differ");
        const std::size_t builds_before_clear = cache.build_count();
        cache.clear();
        check(cache.entries() == 0 && cache.resident_bytes() == 0,
              "clear did not release resident cache state");
        check(cache.build_count() == builds_before_clear &&
              cache.high_water_bytes() >= first_high_water,
              "clear reset cumulative diagnostics");
        auto after_clear = cache.get_or_build(3, [] { return std::string(32, 'c'); });
        check(after_clear->size() == 32 && cache.build_count() == builds_before_clear + 1,
              "cache was not reusable after clear");

        Cache oversize(256, string_bytes);
        auto large = oversize.get_or_build(1, [] { return std::string(2048, 'x'); });
        check(large->size() == 2048 && oversize.entries() == 0 &&
              oversize.resident_bytes() == 0 && oversize.uncached() == 1,
              "oversize value was not returned uncached");
        check(oversize.maximum_entry_bytes() > oversize.maximum_resident_bytes(),
              "oversize maximum charge was not reported");

        Cache overflowing_charge(256, [](const std::string&) {
            return std::numeric_limits<std::size_t>::max();
        });
        overflowing_charge.get_or_build(1, [] { return std::string("tiny"); });
        check(overflowing_charge.maximum_entry_bytes() ==
                  std::numeric_limits<std::size_t>::max() &&
                  overflowing_charge.entries() == 0 && overflowing_charge.uncached() == 1,
              "overflowing size charge was not treated as uncached oversize");

        Cache throwing_builder(4096, string_bytes);
        try {
            throwing_builder.get_or_build(1, []() -> std::string {
                throw std::runtime_error("expected builder failure");
            });
            throw std::logic_error("throwing builder unexpectedly succeeded");
        } catch (const std::runtime_error& error) {
            check(std::string(error.what()) == "expected builder failure",
                  "unexpected builder error");
        }
        check(throwing_builder.entries() == 0 && throwing_builder.resident_bytes() == 0 &&
                  throwing_builder.build_count() == 0,
              "builder failure published a partial entry");
        auto retried = throwing_builder.get_or_build(1, [] { return std::string("retried"); });
        check(*retried == "retried" && throwing_builder.build_count() == 1,
              "failed key could not be retried");

        std::atomic<bool> fail_size{true};
        Cache throwing_size(4096, [&](const std::string& value) {
            if (fail_size.exchange(false)) throw std::runtime_error("expected size failure");
            return string_bytes(value);
        });
        try {
            throwing_size.get_or_build(1, [] { return std::string("sized"); });
            throw std::logic_error("throwing size function unexpectedly succeeded");
        } catch (const std::runtime_error& error) {
            check(std::string(error.what()) == "expected size failure", "unexpected size error");
        }
        check(throwing_size.entries() == 0 && throwing_size.resident_bytes() == 0,
              "size failure published a partial entry");
        check(*throwing_size.get_or_build(1, [] { return std::string("sized"); }) == "sized",
              "size-function failure was not retryable");

        // Force two different keys into one shard. Both builders must enter
        // before either is released; the previous shard-wide flight cannot.
        Cache same_shard(4096, string_bytes);
        BuilderGate same_shard_gate;
        auto left_future = std::async(std::launch::async, [&] {
            return same_shard.get_or_build(1, [&] {
                same_shard_gate.enter_and_wait();
                return std::string("left");
            });
        });
        auto right_future = std::async(std::launch::async, [&] {
            return same_shard.get_or_build(2, [&] {
                same_shard_gate.enter_and_wait();
                return std::string("right");
            });
        });
        const bool distinct_builders_overlapped = same_shard_gate.wait_for_entered(2);
        same_shard_gate.release();
        const auto left = left_future.get();
        const auto right = right_future.get();
        check(distinct_builders_overlapped && *left == "left" && *right == "right" &&
                  same_shard.build_count() == 2 && same_shard.entries() == 2,
              "different keys in one shard did not build concurrently");

        constexpr std::size_t clients = 16;
        Cache single_flight(4096, string_bytes);
        BuilderGate single_flight_gate;
        std::atomic<std::size_t> builder_calls{0};
        std::vector<std::future<Cache::value_ptr>> single_futures;
        single_futures.reserve(clients);
        single_futures.push_back(std::async(std::launch::async, [&] {
            return single_flight.get_or_build(9, [&] {
                builder_calls.fetch_add(1, std::memory_order_relaxed);
                single_flight_gate.enter_and_wait();
                return std::string(128, 'z');
            });
        }));
        const bool single_owner_entered = single_flight_gate.wait_for_entered(1);
        if (single_owner_entered) {
            for (std::size_t i = 1; i < clients; ++i) {
                single_futures.push_back(std::async(std::launch::async, [&] {
                    return single_flight.get_or_build(9, [&] {
                        builder_calls.fetch_add(1, std::memory_order_relaxed);
                        return std::string("unexpected duplicate build");
                    });
                }));
            }
        }
        const bool single_joined = single_owner_entered && wait_until([&] {
            return single_flight.hits() == clients - 1;
        });
        // Always release before checking a timeout so a failing test cannot hang.
        single_flight_gate.release();
        std::vector<Cache::value_ptr> single_values;
        for (auto& future : single_futures) single_values.push_back(future.get());
        check(single_joined && single_values.size() == clients &&
                  builder_calls.load(std::memory_order_relaxed) == 1 &&
                  single_flight.build_count() == 1 &&
                  single_flight.hits() == clients - 1,
              "same-key clients did not deterministically join one flight");
        for (const auto& value : single_values) {
            check(value.get() == single_values.front().get(),
                  "same-key flight returned distinct immutable values");
        }
        single_flight.clear();
        check(single_flight.entries() == 0 && single_flight.resident_bytes() == 0 &&
                  single_flight.build_count() == 1,
              "post-join clear did not preserve diagnostics and release values");

        Cache oversize_flight(256, string_bytes);
        BuilderGate oversize_gate;
        std::atomic<std::size_t> oversize_builders{0};
        std::vector<std::future<Cache::value_ptr>> oversize_futures;
        oversize_futures.reserve(clients);
        oversize_futures.push_back(std::async(std::launch::async, [&] {
            return oversize_flight.get_or_build(5, [&] {
                oversize_builders.fetch_add(1, std::memory_order_relaxed);
                oversize_gate.enter_and_wait();
                return std::string(2048, 'o');
            });
        }));
        const bool oversize_owner_entered = oversize_gate.wait_for_entered(1);
        if (oversize_owner_entered) {
            for (std::size_t i = 1; i < clients; ++i) {
                oversize_futures.push_back(std::async(std::launch::async, [&] {
                    return oversize_flight.get_or_build(5, [&] {
                        oversize_builders.fetch_add(1, std::memory_order_relaxed);
                        return std::string("unexpected duplicate build");
                    });
                }));
            }
        }
        const bool oversize_joined = oversize_owner_entered && wait_until([&] {
            return oversize_flight.hits() == clients - 1;
        });
        oversize_gate.release();
        std::vector<Cache::value_ptr> oversize_values;
        for (auto& future : oversize_futures) oversize_values.push_back(future.get());
        check(oversize_joined && oversize_values.size() == clients &&
                  oversize_builders.load(std::memory_order_relaxed) == 1 &&
                  oversize_flight.build_count() == 1 && oversize_flight.uncached() == 1 &&
                  oversize_flight.entries() == 0 && oversize_flight.resident_bytes() == 0,
              "oversize clients did not share one uncached flight");
        for (const auto& value : oversize_values) {
            check(value.get() == oversize_values.front().get(),
                  "oversize flight returned distinct transient leases");
        }

        Cache failed_flight(4096, string_bytes);
        BuilderGate failed_gate;
        std::atomic<std::size_t> failed_builders{0};
        std::vector<std::future<Cache::value_ptr>> failed_futures;
        failed_futures.reserve(clients);
        failed_futures.push_back(std::async(std::launch::async, [&] {
            return failed_flight.get_or_build(6, [&]() -> std::string {
                failed_builders.fetch_add(1, std::memory_order_relaxed);
                failed_gate.enter_and_wait();
                throw std::runtime_error("expected flight failure");
            });
        }));
        const bool failed_owner_entered = failed_gate.wait_for_entered(1);
        if (failed_owner_entered) {
            for (std::size_t i = 1; i < clients; ++i) {
                failed_futures.push_back(std::async(std::launch::async, [&] {
                    return failed_flight.get_or_build(6, [&] {
                        failed_builders.fetch_add(1, std::memory_order_relaxed);
                        return std::string("unexpected duplicate build");
                    });
                }));
            }
        }
        const bool failed_joined = failed_owner_entered && wait_until([&] {
            return failed_flight.hits() == clients - 1;
        });
        failed_gate.release();
        std::size_t failed_errors = 0;
        for (auto& future : failed_futures) {
            try {
                static_cast<void>(future.get());
            } catch (const std::runtime_error& error) {
                if (std::string(error.what()) == "expected flight failure") ++failed_errors;
                else throw;
            }
        }
        check(failed_joined && failed_errors == clients &&
                  failed_builders.load(std::memory_order_relaxed) == 1 &&
                  failed_flight.hits() == clients - 1 && failed_flight.build_count() == 0 &&
                  failed_flight.entries() == 0 && failed_flight.resident_bytes() == 0,
              "builder exception did not resolve every joined flight exactly once");
        check(*failed_flight.get_or_build(6, [] { return std::string("retried flight"); }) ==
                  "retried flight" && failed_flight.build_count() == 1,
              "builder-exception flight was not retryable");

        BuilderGate size_gate;
        std::atomic<bool> fail_flight_size{true};
        Cache failed_size_flight(4096, [&](const std::string& value) {
            if (fail_flight_size.exchange(false, std::memory_order_relaxed)) {
                size_gate.enter_and_wait();
                throw std::runtime_error("expected flight size failure");
            }
            return string_bytes(value);
        });
        std::atomic<std::size_t> size_builders{0};
        std::vector<std::future<Cache::value_ptr>> size_futures;
        size_futures.reserve(clients);
        size_futures.push_back(std::async(std::launch::async, [&] {
            return failed_size_flight.get_or_build(7, [&] {
                size_builders.fetch_add(1, std::memory_order_relaxed);
                return std::string("sized flight");
            });
        }));
        const bool size_owner_entered = size_gate.wait_for_entered(1);
        if (size_owner_entered) {
            for (std::size_t i = 1; i < clients; ++i) {
                size_futures.push_back(std::async(std::launch::async, [&] {
                    return failed_size_flight.get_or_build(7, [&] {
                        size_builders.fetch_add(1, std::memory_order_relaxed);
                        return std::string("unexpected duplicate build");
                    });
                }));
            }
        }
        const bool size_joined = size_owner_entered && wait_until([&] {
            return failed_size_flight.hits() == clients - 1;
        });
        size_gate.release();
        std::size_t size_errors = 0;
        for (auto& future : size_futures) {
            try {
                static_cast<void>(future.get());
            } catch (const std::runtime_error& error) {
                if (std::string(error.what()) == "expected flight size failure") ++size_errors;
                else throw;
            }
        }
        check(size_joined && size_errors == clients &&
                  size_builders.load(std::memory_order_relaxed) == 1 &&
                  failed_size_flight.hits() == clients - 1 &&
                  failed_size_flight.build_count() == 1 && failed_size_flight.entries() == 0 &&
                  failed_size_flight.resident_bytes() == 0,
              "value-size exception did not resolve and erase its flight");
        check(*failed_size_flight.get_or_build(7, [] { return std::string("retried size"); }) ==
                  "retried size" && failed_size_flight.build_count() == 2,
              "value-size-exception flight was not retryable");

        using PublicationCache =
            pancollapse::BoundedReferenceCache<ThrowingKey, std::string, 1, ThrowingKeyHash>;
        const auto throw_on_copy = std::make_shared<std::atomic<bool>>(false);
        const ThrowingKey publication_key(8, throw_on_copy);
        PublicationCache failed_publication(4096, string_bytes);
        BuilderGate publication_gate;
        std::atomic<std::size_t> publication_builders{0};
        std::vector<std::future<PublicationCache::value_ptr>> publication_futures;
        publication_futures.reserve(clients);
        publication_futures.push_back(std::async(std::launch::async, [&] {
            return failed_publication.get_or_build(publication_key, [&] {
                publication_builders.fetch_add(1, std::memory_order_relaxed);
                publication_gate.enter_and_wait();
                throw_on_copy->store(true, std::memory_order_relaxed);
                return std::string("publication");
            });
        }));
        const bool publication_owner_entered = publication_gate.wait_for_entered(1);
        if (publication_owner_entered) {
            for (std::size_t i = 1; i < clients; ++i) {
                publication_futures.push_back(std::async(std::launch::async, [&] {
                    return failed_publication.get_or_build(publication_key, [&] {
                        publication_builders.fetch_add(1, std::memory_order_relaxed);
                        return std::string("unexpected duplicate build");
                    });
                }));
            }
        }
        const bool publication_joined = publication_owner_entered && wait_until([&] {
            return failed_publication.hits() == clients - 1;
        });
        publication_gate.release();
        std::size_t publication_errors = 0;
        for (auto& future : publication_futures) {
            try {
                static_cast<void>(future.get());
            } catch (const std::runtime_error& error) {
                if (std::string(error.what()) == "expected publication failure") {
                    ++publication_errors;
                } else {
                    throw;
                }
            }
        }
        check(publication_joined && publication_errors == clients &&
                  publication_builders.load(std::memory_order_relaxed) == 1 &&
                  failed_publication.hits() == clients - 1 &&
                  failed_publication.build_count() == 1 && failed_publication.entries() == 0 &&
                  failed_publication.resident_bytes() == 0,
              "publication exception did not roll back and resolve its flight");
        throw_on_copy->store(false, std::memory_order_relaxed);
        check(*failed_publication.get_or_build(publication_key,
                  [] { return std::string("retried publication"); }) == "retried publication" &&
                  failed_publication.build_count() == 2 && failed_publication.entries() == 1,
              "publication-exception flight was not retryable");

        using ShardedCache = pancollapse::BoundedShardedCache<int, std::string, 1>;
        ShardedCache racing_cache(16, 4096, string_bytes);
        BuilderGate duplicate_gate;
        auto race_builder = [&] {
            return racing_cache.get_or_build(1, [&] {
                duplicate_gate.enter_and_wait();
                return std::string("same");
            });
        };
        auto first_racer = std::async(std::launch::async, race_builder);
        auto second_racer = std::async(std::launch::async, race_builder);
        const bool both_racers_entered = duplicate_gate.wait_for_entered(2);
        duplicate_gate.release();
        const auto first_race_value = first_racer.get();
        const auto second_race_value = second_racer.get();
        check(both_racers_entered && first_race_value == second_race_value &&
                  racing_cache.entries() == 1 && racing_cache.hits() == 1 &&
                  racing_cache.misses() == 1 && racing_cache.duplicate_builds() == 1,
              "racing completed builders were confused with avoided work");
        const auto ready_hit = racing_cache.get_or_build(1, []() -> std::string {
            throw std::runtime_error("ready hit rebuilt its value");
        });
        check(ready_hit == first_race_value && racing_cache.hits() == 2 &&
                  racing_cache.duplicate_builds() == 1 && racing_cache.misses() == 1,
              "ready hit changed the duplicate-build accounting");

        ShardedCache pressure_cache(16, 4096, string_bytes);
        const auto pressure_lease = pressure_cache.get_or_build(
            1, [] { return std::string(128, 'p'); });
        BuilderGate pressure_gate;
        auto pressure_builder = std::async(std::launch::async, [&] {
            return pressure_cache.get_or_build(2, [&] {
                pressure_gate.enter_and_wait();
                return std::string(128, 'q');
            });
        });
        const bool pressure_builder_entered = pressure_gate.wait_for_entered(1);
        const auto pressure_clear = pressure_cache.disable_retention_and_clear();
        pressure_gate.release();
        const auto pressure_value = pressure_builder.get();
        check(pressure_builder_entered && pressure_clear.entries == 1 &&
                  pressure_clear.charged_bytes > 0 && !pressure_cache.retention_enabled() &&
                  pressure_cache.entries() == 0 && pressure_cache.charged_bytes() == 0 &&
                  *pressure_lease == std::string(128, 'p') &&
                  *pressure_value == std::string(128, 'q'),
              "pressure clear invalidated a lease or admitted a late builder");
        const auto repeated_clear = pressure_cache.disable_retention_and_clear();
        const auto uncached_after_clear = pressure_cache.get_or_build(
            3, [] { return std::string("uncached"); });
        check(repeated_clear.entries == 0 && repeated_clear.charged_bytes == 0 &&
                  *uncached_after_clear == "uncached" && pressure_cache.entries() == 0 &&
                  pressure_cache.charged_bytes() == 0 && pressure_cache.uncached() == 2,
              "disabled cache was not idempotent or retained a later value");

        std::cout << "PASS: byte bound, FIFO eviction, active lease, oversize bypass, "
                     "post-join clear, per-key flights, same-shard concurrency, failure fanout, "
                     "and permanent sharded-cache retirement\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
