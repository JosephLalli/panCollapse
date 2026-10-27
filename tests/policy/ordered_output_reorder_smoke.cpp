#include "ordered_output_reorder.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using pancollapse::output::OrderedOutputReorder;

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

// Workers take ordinals from a shared counter, as count workers take reads from the FIFO, and
// finish them after random delays so deposits arrive out of order. Commits must still run in
// ordinal order, one at a time, and pending must never exceed the bound (plus the one ordinal
// the bound always admits).
void test_out_of_order_deposits_commit_in_order() {
    constexpr std::size_t kReads = 20000;
    constexpr std::size_t kWorkers = 16;
    constexpr std::size_t kBound = 32;
    OrderedOutputReorder reorder(kBound);
    std::atomic<std::size_t> next_read{0};
    std::vector<std::size_t> committed;
    committed.reserve(kReads);
    std::atomic<int> active_commits{0};
    std::atomic<bool> overlapping_commit{false};
    std::vector<std::thread> workers;
    for (std::size_t worker = 0; worker < kWorkers; ++worker) {
        workers.emplace_back([&, worker] {
            std::mt19937 rng(static_cast<unsigned>(worker) * 7919U + 1U);
            while (true) {
                const std::size_t ordinal = next_read.fetch_add(1);
                if (ordinal >= kReads) break;
                // A few reads are dense: they take far longer than the rest.
                const int delay_us = rng() % 97 == 0 ? 3000 : static_cast<int>(rng() % 40);
                std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
                OrderedOutputReorder::Actions actions;
                if (ordinal % 5 != 0) {  // some reads write nothing and still deposit
                    actions.push_back([&, ordinal] {
                        if (active_commits.fetch_add(1) != 0) overlapping_commit = true;
                        committed.push_back(ordinal);
                        active_commits.fetch_sub(1);
                    });
                }
                reorder.deposit(ordinal, std::move(actions));
            }
        });
    }
    for (std::thread& worker : workers) worker.join();
    check(!overlapping_commit, "two commits ran at once");
    check(reorder.committed() == kReads, "not every ordinal committed");
    std::vector<std::size_t> expected;
    for (std::size_t ordinal = 0; ordinal < kReads; ++ordinal) {
        if (ordinal % 5 != 0) expected.push_back(ordinal);
    }
    check(committed == expected, "commits out of ordinal order");
    check(reorder.peak_pending() <= kBound + 1, "pending exceeded its bound: " +
          std::to_string(reorder.peak_pending()));
    check(reorder.peak_pending() > 1, "deposits never arrived out of order");
}

// A read that fails never deposits: nothing after it commits, and a worker blocked on the
// bound is released by abort() with an error instead of waiting forever.
void test_missing_ordinal_stops_commits_and_abort_releases_waiters() {
    OrderedOutputReorder reorder(2);
    std::vector<std::size_t> committed;
    reorder.deposit(0, {[&] { committed.push_back(0); }});
    // Ordinal 1 fails and never deposits; 2 and 3 fill the bound.
    reorder.deposit(2, {[&] { committed.push_back(2); }});
    reorder.deposit(3, {[&] { committed.push_back(3); }});
    std::atomic<bool> released{false};
    std::thread blocked([&] {
        try {
            reorder.deposit(4, {[&] { committed.push_back(4); }});
        } catch (const std::runtime_error&) {
            released = true;
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    check(!released, "a deposit past the bound did not block");
    reorder.abort();
    blocked.join();
    check(released, "abort did not release the blocked deposit with an error");
    check(committed == std::vector<std::size_t>{0}, "committed past a missing ordinal");
    check(reorder.committed() == 1, "committed count past a missing ordinal");
}

// A commit action that throws propagates to the depositor and stops later commits.
void test_throwing_commit_aborts() {
    OrderedOutputReorder reorder(8);
    std::vector<std::size_t> committed;
    reorder.deposit(1, {[&] { committed.push_back(1); }});
    bool threw = false;
    try {
        reorder.deposit(0, {[] { throw std::runtime_error("append failed"); }});
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "append failed";
    }
    check(threw, "a throwing commit did not reach its depositor");
    check(committed.empty(), "committed after a throwing commit");
    bool rejected = false;
    try {
        reorder.deposit(2, {});
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, "deposits accepted after a failed commit");
}

}  // namespace

int main() {
    try {
        test_out_of_order_deposits_commit_in_order();
        test_missing_ordinal_stops_commits_and_abort_releases_waiters();
        test_throwing_commit_aborts();
    } catch (const std::exception& error) {
        std::cerr << "ordered_output_reorder_smoke: " << error.what() << '\n';
        return 1;
    }
    std::cout << "ordered_output_reorder_smoke: ok\n";
    return 0;
}
