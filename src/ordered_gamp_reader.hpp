#pragma once

#include <vg/io/protobuf_iterator.hpp>
#include <vg/io/registry.hpp>
#include <vg/vg.pb.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <istream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <time.h>
#endif

namespace pancollapse::input {

struct OrderedGampReaderOptions {
    std::size_t decoder_threads = 4;
    std::size_t decompression_threads = 2;
    std::size_t batch_bytes = 1U << 20;
    std::size_t batch_records = 512;
    std::size_t max_inflight_batches = 8;
    std::size_t max_buffer_bytes = 64U << 20;

    // Test/diagnostic hook. It runs on a decoder worker immediately before
    // decoding the batch with this zero-based ordinal.
    std::function<void(std::uint64_t)> before_decode;
};

struct OrderedGampReaderStats {
    std::uint64_t batches = 0;
    std::uint64_t records = 0;
    std::uint64_t raw_bytes = 0;
    std::uint64_t decoded_bytes = 0;
    std::size_t peak_raw_bytes = 0;
    std::size_t peak_decoded_bytes = 0;
    std::size_t peak_buffer_bytes = 0;
    std::uint64_t budget_oversize_records = 0;
    double decoder_cpu_seconds = 0.0;
    // Producer-thread wall clock, partitioned. The producer is serial, so these three add
    // up to nearly the whole read phase and say which stage is the critical path.
    double producer_input_seconds = 0.0;    // pulling and framing raw records
    double producer_commit_wait_seconds = 0.0;  // blocked on the OLDEST batch decoding
    double producer_callback_seconds = 0.0; // grouping and submitting, per record
};

namespace detail {

using GampRecord = vg::MultipathAlignment;

struct RawGampRecord {
    std::unique_ptr<std::string> data;
    bool exceeds_raw_budget = false;
};

struct GampBatchSlot {
    std::uint64_t ordinal = 0;
    std::vector<RawGampRecord> raw;
    std::size_t raw_payload_bytes = 0;
    std::size_t raw_string_allocation_bytes = 0;
    std::size_t raw_accounted_bytes = 0;

    std::vector<GampRecord> decoded;
    std::size_t decoded_accounted_bytes = 0;
    std::exception_ptr failure;
    bool complete = false;
};

inline std::size_t saturated_add(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        return std::numeric_limits<std::size_t>::max();
    }
    return left + right;
}

inline std::size_t saturated_multiply(std::size_t left, std::size_t right) {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
        return std::numeric_limits<std::size_t>::max();
    }
    return left * right;
}

inline std::uint64_t saturated_add_u64(std::uint64_t left, std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return left + right;
}

inline std::size_t raw_batch_allocation(const GampBatchSlot& slot) {
    std::size_t result = sizeof(GampBatchSlot);
    result = saturated_add(result,
                           saturated_multiply(slot.raw.capacity(), sizeof(RawGampRecord)));
    return saturated_add(result, slot.raw_string_allocation_bytes);
}

inline void set_decoder_thread_name() noexcept {
#if defined(__linux__)
    (void)pthread_setname_np(pthread_self(), "pc-decode");
#endif
}

inline std::uint64_t thread_cpu_now_ns() noexcept {
#if defined(__linux__) && defined(CLOCK_THREAD_CPUTIME_ID)
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) == 0) {
        return static_cast<std::uint64_t>(value.tv_sec) * 1'000'000'000ULL
            + static_cast<std::uint64_t>(value.tv_nsec);
    }
#endif
    return 0;
}

class OrderedGampDecoderPool {
public:
    explicit OrderedGampDecoderPool(const OrderedGampReaderOptions& options)
        : before_decode_(options.before_decode), max_buffer_bytes_(options.max_buffer_bytes) {
        try {
            workers_.reserve(options.decoder_threads);
            for (std::size_t i = 0; i < options.decoder_threads; ++i) {
                workers_.emplace_back([this] { worker_loop(); });
            }
        } catch (...) {
            request_stop();
            join();
            throw;
        }
    }

    OrderedGampDecoderPool(const OrderedGampDecoderPool&) = delete;
    OrderedGampDecoderPool& operator=(const OrderedGampDecoderPool&) = delete;

    ~OrderedGampDecoderPool() {
        request_stop();
        join();
    }

    void add_raw_bytes(std::size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        raw_bytes_ = saturated_add(raw_bytes_, bytes);
        update_peaks_locked();
    }

    void add_decoded_bytes(std::size_t bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        decoded_bytes_ = saturated_add(decoded_bytes_, bytes);
        update_peaks_locked();
    }

    void discard_decoded_bytes(std::size_t bytes) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        decoded_bytes_ = bytes > decoded_bytes_ ? 0 : decoded_bytes_ - bytes;
    }

    std::size_t buffered_bytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return saturated_add(raw_bytes_, decoded_bytes_);
    }

    void submit(const std::shared_ptr<GampBatchSlot>& slot) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("ordered GAMP decoder pool is stopped");
            }
            work_.push_back(slot);
        }
        work_available_.notify_one();
    }

    void wait_until_complete(const std::shared_ptr<GampBatchSlot>& slot) {
        std::unique_lock<std::mutex> lock(mutex_);
        state_changed_.wait(lock, [&] { return slot->complete || stopping_; });
        if (!slot->complete) {
            throw std::runtime_error("ordered GAMP decoder pool stopped before batch completion");
        }
    }

    void release_decoded(const std::shared_ptr<GampBatchSlot>& slot) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slot->decoded_accounted_bytes > decoded_bytes_) {
            decoded_bytes_ = 0;
        } else {
            decoded_bytes_ -= slot->decoded_accounted_bytes;
        }
        slot->decoded_accounted_bytes = 0;
    }

    void finish() {
        request_stop();
        join();
    }

    void cancel() noexcept {
        request_stop();
        join();
    }

    void fill_stats(OrderedGampReaderStats& stats) const {
        std::lock_guard<std::mutex> lock(mutex_);
        stats.decoded_bytes = decoded_bytes_total_;
        stats.peak_raw_bytes = peak_raw_bytes_;
        stats.peak_decoded_bytes = peak_decoded_bytes_;
        stats.peak_buffer_bytes = peak_buffer_bytes_;
        stats.budget_oversize_records = budget_oversize_records_;
        stats.decoder_cpu_seconds = static_cast<double>(decoder_cpu_ns_.load()) / 1.0e9;
    }

private:
    void request_stop() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        work_available_.notify_all();
        state_changed_.notify_all();
    }

    void join() noexcept {
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    bool stop_requested() const {
        return stopping_.load(std::memory_order_relaxed);
    }

    void update_peaks_locked() {
        peak_raw_bytes_ = std::max(peak_raw_bytes_, raw_bytes_);
        peak_decoded_bytes_ = std::max(peak_decoded_bytes_, decoded_bytes_);
        peak_buffer_bytes_ = std::max(
            peak_buffer_bytes_, saturated_add(raw_bytes_, decoded_bytes_));
    }

    void publish(const std::shared_ptr<GampBatchSlot>& slot,
                 std::vector<GampRecord>&& decoded,
                 std::size_t decoded_allocation,
                 std::size_t decoded_space_used,
                 std::uint64_t oversized_records,
                 std::exception_ptr failure) {
        // Decoded allocation was accounted as it grew. Publish it while the
        // serialized batch is still alive, then release the raw storage before
        // making the slot committable.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_.load(std::memory_order_relaxed)) {
                decoded_bytes_ = decoded_allocation > decoded_bytes_
                    ? 0 : decoded_bytes_ - decoded_allocation;
                return;
            }
            slot->decoded = std::move(decoded);
            slot->decoded_accounted_bytes = decoded_allocation;
            slot->failure = std::move(failure);
            decoded_bytes_total_ = saturated_add_u64(
                decoded_bytes_total_, static_cast<std::uint64_t>(decoded_space_used));
            budget_oversize_records_ = saturated_add_u64(
                budget_oversize_records_, oversized_records);
            update_peaks_locked();
        }

        // Release serialized storage before making the result committable.
        // The caller therefore never observes a complete slot whose raw memory
        // is still retained.
        std::vector<RawGampRecord>().swap(slot->raw);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (slot->raw_accounted_bytes > raw_bytes_) {
                raw_bytes_ = 0;
            } else {
                raw_bytes_ -= slot->raw_accounted_bytes;
            }
            slot->raw_accounted_bytes = 0;
            slot->complete = true;
            update_peaks_locked();
        }
        state_changed_.notify_all();
    }

    void worker_loop() noexcept {
        set_decoder_thread_name();
        const std::uint64_t cpu_started = thread_cpu_now_ns();

        while (true) {
            std::shared_ptr<GampBatchSlot> slot;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                work_available_.wait(lock, [&] { return stopping_ || !work_.empty(); });
                if (stopping_) {
                    break;
                }
                slot = std::move(work_.front());
                work_.pop_front();
            }

            std::vector<GampRecord> decoded;
            std::size_t decoded_allocation = 0;
            std::size_t decoded_space_used = 0;
            std::uint64_t oversized_records = 0;
            std::exception_ptr failure;
            bool cancelled = false;

            try {
                if (before_decode_) {
                    before_decode_(slot->ordinal);
                }
                decoded.reserve(slot->raw.size());
                decoded_allocation = saturated_add(
                    sizeof(GampBatchSlot),
                    saturated_multiply(decoded.capacity(), sizeof(GampRecord)));
                add_decoded_bytes(decoded_allocation);
                for (const RawGampRecord& raw_record : slot->raw) {
                    if (stop_requested()) {
                        cancelled = true;
                        break;
                    }
                    decoded.emplace_back();
                    if (!vg::io::ProtobufIterator<GampRecord>::parse_from_string(
                            decoded.back(), *raw_record.data)) {
                        decoded.pop_back();
                        throw std::runtime_error(
                            "[io::ProtobufIterator] could not parse message");
                    }
                    const std::size_t record_space = decoded.back().SpaceUsedLong();
                    decoded_space_used = saturated_add(decoded_space_used, record_space);
                    const std::size_t dynamic_space = record_space > sizeof(GampRecord)
                        ? record_space - sizeof(GampRecord)
                        : 0;
                    decoded_allocation = saturated_add(decoded_allocation, dynamic_space);
                    add_decoded_bytes(dynamic_space);
                    if (raw_record.exceeds_raw_budget ||
                        record_space > max_buffer_bytes_) {
                        ++oversized_records;
                    }
                }
            } catch (...) {
                failure = std::current_exception();
            }

            if (cancelled || stop_requested()) {
                discard_decoded_bytes(decoded_allocation);
                break;
            }
            publish(slot, std::move(decoded), decoded_allocation, decoded_space_used,
                    oversized_records, std::move(failure));
        }

        const std::uint64_t cpu_finished = thread_cpu_now_ns();
        if (cpu_finished >= cpu_started) {
            decoder_cpu_ns_.fetch_add(cpu_finished - cpu_started,
                                      std::memory_order_relaxed);
        }
    }

    std::function<void(std::uint64_t)> before_decode_;
    std::size_t max_buffer_bytes_;

    mutable std::mutex mutex_;
    std::condition_variable work_available_;
    std::condition_variable state_changed_;
    std::deque<std::shared_ptr<GampBatchSlot>> work_;
    std::atomic<bool> stopping_{false};
    std::vector<std::thread> workers_;

    std::size_t raw_bytes_ = 0;
    std::size_t decoded_bytes_ = 0;
    std::size_t peak_raw_bytes_ = 0;
    std::size_t peak_decoded_bytes_ = 0;
    std::size_t peak_buffer_bytes_ = 0;
    std::uint64_t decoded_bytes_total_ = 0;
    std::uint64_t budget_oversize_records_ = 0;
    std::atomic<std::uint64_t> decoder_cpu_ns_{0};
};

inline void validate_options(const OrderedGampReaderOptions& options) {
    if (options.decoder_threads == 0) {
        return;
    }
    if (options.batch_bytes == 0) {
        throw std::invalid_argument("ordered GAMP batch_bytes must be positive");
    }
    if (options.batch_records == 0) {
        throw std::invalid_argument("ordered GAMP batch_records must be positive");
    }
    if (options.max_inflight_batches == 0) {
        throw std::invalid_argument(
            "ordered GAMP max_inflight_batches must be positive");
    }
    if (options.max_buffer_bytes == 0) {
        throw std::invalid_argument("ordered GAMP max_buffer_bytes must be positive");
    }
}

}  // namespace detail

// Read a GAMP stream with one caller-thread framing/commit owner and a bounded
// protobuf decoder pool. The callback is invoked sequentially, in input order,
// on the calling thread and may move from its MultipathAlignment argument.
//
// max_buffer_bytes is an admission watermark for adapter-owned serialized and
// decoded batches. One raw record larger than the budget is admitted alone.
// Batches already admitted can cross the watermark as they decode, and each
// decoder can have one currently expanding protobuf that is not measurable
// until parsing finishes. max_inflight_batches bounds their count; this is not
// a hard RSS limit. BGZF internals, MessageIterator's current record, and memory
// moved into callback-owned read groups are also outside the watermark.
template<class Callback>
OrderedGampReaderStats ordered_gamp_for_each(
    std::istream& input,
    Callback&& callback,
    const OrderedGampReaderOptions& options = {}) {
    static_assert(std::is_invocable_v<Callback&, vg::MultipathAlignment&>,
                  "ordered GAMP callback must accept vg::MultipathAlignment&");

    detail::validate_options(options);
    OrderedGampReaderStats stats;

    // Keep the existing vg serial reader as the exact low-thread fallback.
    if (options.decoder_threads == 0) {
        for (vg::io::ProtobufIterator<vg::MultipathAlignment> iterator(input);
             iterator.has_current(); ++iterator) {
            vg::MultipathAlignment& record = *iterator;
            stats.batches = 1;
            const std::size_t decoded_size = record.SpaceUsedLong();
            stats.decoded_bytes = detail::saturated_add_u64(
                stats.decoded_bytes, static_cast<std::uint64_t>(decoded_size));
            stats.peak_decoded_bytes = std::max(stats.peak_decoded_bytes, decoded_size);
            stats.peak_buffer_bytes = std::max(stats.peak_buffer_bytes, decoded_size);
            std::invoke(callback, record);
            ++stats.records;
        }
        return stats;
    }

    // Constructing MessageIterator can read and reject the beginning of the
    // stream. Do this before starting decoder workers. While it exists it is
    // the sole owner of all access to input.
    vg::io::MessageIterator message_iterator(
        input, false, options.decompression_threads);
    detail::OrderedGampDecoderPool pool(options);
    std::deque<std::shared_ptr<detail::GampBatchSlot>> inflight;
    std::shared_ptr<detail::GampBatchSlot> building;
    std::uint64_t next_batch_ordinal = 0;

    auto seal_building = [&] {
        if (!building) {
            return;
        }
        pool.submit(building);
        ++stats.batches;
        building.reset();
    };

    auto release_front = [&] {
        const std::shared_ptr<detail::GampBatchSlot> slot = inflight.front();
        pool.release_decoded(slot);
        std::vector<detail::GampRecord>().swap(slot->decoded);
        inflight.pop_front();
    };

    auto commit_front = [&] {
        const std::shared_ptr<detail::GampBatchSlot> slot = inflight.front();
        const auto wait_started = std::chrono::steady_clock::now();
        pool.wait_until_complete(slot);
        const auto wait_ended = std::chrono::steady_clock::now();
        stats.producer_commit_wait_seconds +=
            std::chrono::duration<double>(wait_ended - wait_started).count();
        try {
            for (detail::GampRecord& record : slot->decoded) {
                std::invoke(callback, record);
                ++stats.records;
            }
            stats.producer_callback_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() - wait_ended).count();
        } catch (...) {
            release_front();
            throw;
        }
        const std::exception_ptr failure = slot->failure;
        release_front();
        if (failure) {
            std::rethrow_exception(failure);
        }
    };

    auto drain_all = [&] {
        seal_building();
        while (!inflight.empty()) {
            commit_front();
        }
    };

    auto wait_for_batch_slot = [&] {
        while (inflight.size() >= options.max_inflight_batches) {
            commit_front();
        }
    };

    auto start_batch = [&] {
        wait_for_batch_slot();
        building = std::make_shared<detail::GampBatchSlot>();
        building->ordinal = next_batch_ordinal++;
        inflight.push_back(building);  // Reserve its ordered result slot first.
    };

    auto append_record = [&](std::unique_ptr<std::string> data) {
        const std::size_t string_allocation = detail::saturated_add(
            sizeof(std::string), detail::saturated_add(data->capacity(), 1));
        std::size_t standalone_allocation = sizeof(detail::GampBatchSlot);
        standalone_allocation = detail::saturated_add(
            standalone_allocation, sizeof(detail::RawGampRecord));
        standalone_allocation = detail::saturated_add(
            standalone_allocation, string_allocation);
        const bool oversized = standalone_allocation > options.max_buffer_bytes;

        // A raw record that alone exceeds the budget is isolated. For ordinary
        // records, seal the partial batch and commit old slots until admission
        // is back under the shared raw-plus-decoded budget.
        std::size_t projected_growth = detail::saturated_add(
            sizeof(detail::RawGampRecord), string_allocation);
        if (!building) {
            projected_growth = detail::saturated_add(
                projected_growth, sizeof(detail::GampBatchSlot));
        }
        if (oversized) {
            seal_building();
            while (!inflight.empty()) {
                commit_front();
            }
        } else if (detail::saturated_add(pool.buffered_bytes(), projected_growth)
                   > options.max_buffer_bytes) {
            seal_building();
            while (!inflight.empty() &&
                   detail::saturated_add(pool.buffered_bytes(), projected_growth)
                       > options.max_buffer_bytes) {
                commit_front();
            }
        }

        if (!building) {
            start_batch();
        }

        const std::size_t old_accounted = building->raw_accounted_bytes;
        const std::size_t payload_size = data->size();
        building->raw_payload_bytes = detail::saturated_add(
            building->raw_payload_bytes, payload_size);
        building->raw_string_allocation_bytes = detail::saturated_add(
            building->raw_string_allocation_bytes, string_allocation);
        building->raw.push_back({std::move(data), oversized});
        building->raw_accounted_bytes = detail::raw_batch_allocation(*building);
        if (building->raw_accounted_bytes >= old_accounted) {
            pool.add_raw_bytes(building->raw_accounted_bytes - old_accounted);
        }
        stats.raw_bytes = detail::saturated_add_u64(
            stats.raw_bytes, static_cast<std::uint64_t>(payload_size));

        if (oversized || building->raw_payload_bytes >= options.batch_bytes ||
            building->raw.size() >= options.batch_records) {
            seal_building();
        }
    };

    std::exception_ptr reader_failure;
    try {
        auto input_started = std::chrono::steady_clock::now();
        while (message_iterator.has_current()) {
            auto& tagged = *message_iterator;
            const bool right_tag =
                vg::io::Registry::check_protobuf_tag<vg::MultipathAlignment>(tagged.first);
            if (right_tag && tagged.second) {
                // Admission can commit older slots. A callback or decoder
                // failure from that ordered commit is already at its exact
                // serial position and must escape directly to cancellation;
                // it is not a deferred framing failure.
                append_record(std::move(tagged.second));
            }
            try {
                // Advance separately from moving the current record. If this
                // throws, the accepted current record still precedes the reader
                // failure and is committed before that failure is rethrown.
                message_iterator.advance();
            } catch (...) {
                reader_failure = std::current_exception();
                break;
            }
        }
        // Whole-loop wall minus the committing done inside it is the input cost.
        stats.producer_input_seconds +=
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - input_started).count()
            - stats.producer_commit_wait_seconds - stats.producer_callback_seconds;

        drain_all();
        if (reader_failure) {
            std::rethrow_exception(reader_failure);
        }
        pool.finish();
        pool.fill_stats(stats);
        return stats;
    } catch (...) {
        pool.cancel();
        throw;
    }
}

}  // namespace pancollapse::input
