#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pancollapse::output {

// D090: count's ordered sinks (RAD records, compatibility rows, their warnings) are captured per
// read and committed here in ordinal order, so a worker that finishes a later read deposits and
// moves on instead of parking behind a dense earlier read. Whoever deposits the next-expected
// ordinal commits every consecutive ready read. The pending bound never blocks the next-expected
// ordinal: workers take reads in ordinal order, so that ordinal is always held by a running
// worker, and the stage cannot deadlock. A read that fails never deposits, so committed output
// stays a strict ordinal prefix, as with OrderedOutputCoordinator.
class OrderedOutputReorder {
public:
    using Actions = std::vector<std::function<void()>>;

    explicit OrderedOutputReorder(std::size_t max_pending) : max_pending_(max_pending) {}

    // Returns the nanoseconds this call spent blocked on the pending bound.
    std::uint64_t deposit(std::size_t ordinal, Actions actions) {
        std::unique_lock<std::mutex> lock(mutex_);
        std::uint64_t blocked_ns = 0;
        if (!ready_to_accept(ordinal)) {
            const auto blocked_started = std::chrono::steady_clock::now();
            cv_.wait(lock, [&] { return ready_to_accept(ordinal); });
            blocked_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - blocked_started).count());
        }
        if (aborted_) {
            throw std::runtime_error("ordered output aborted after a worker failure");
        }
        pending_.emplace(ordinal, std::move(actions));
        peak_pending_ = std::max(peak_pending_, pending_.size());
        if (committing_ || ordinal != next_ordinal_) {
            return blocked_ns;
        }
        committing_ = true;
        for (auto ready = pending_.find(next_ordinal_); ready != pending_.end();
             ready = pending_.find(next_ordinal_)) {
            Actions batch = std::move(ready->second);
            pending_.erase(ready);
            lock.unlock();
            try {
                for (const std::function<void()>& action : batch) {
                    action();
                }
            } catch (...) {
                lock.lock();
                committing_ = false;
                aborted_ = true;
                cv_.notify_all();
                throw;
            }
            lock.lock();
            ++next_ordinal_;
            cv_.notify_all();
        }
        committing_ = false;
        return blocked_ns;
    }

    void abort() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            aborted_ = true;
        }
        cv_.notify_all();
    }

    std::size_t committed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return next_ordinal_;
    }

    std::size_t peak_pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return peak_pending_;
    }

private:
    bool ready_to_accept(std::size_t ordinal) const {
        return aborted_ || ordinal == next_ordinal_ || pending_.size() < max_pending_;
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t max_pending_;
    std::map<std::size_t, Actions> pending_;
    std::size_t next_ordinal_ = 0;
    std::size_t peak_pending_ = 0;
    bool committing_ = false;
    bool aborted_ = false;
};

}  // namespace pancollapse::output
