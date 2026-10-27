#include "ordered_gamp_reader.hpp"

#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::vector<vg::MultipathAlignment> fixture() {
    std::vector<vg::MultipathAlignment> records;
    for (size_t group = 0; group < 9; ++group) {
        for (size_t member = 0; member < 1 + (group % 3); ++member) {
            vg::MultipathAlignment record;
            record.set_name("read" + std::to_string(group));
            record.set_sequence("ACGT" + std::to_string(member));
            record.set_mapping_quality(static_cast<int>(group + member));
            records.push_back(std::move(record));
        }
    }
    return records;
}

std::string encode(const std::vector<vg::MultipathAlignment>& records, bool compressed) {
    std::ostringstream out(std::ios::binary);
    check(vg::io::write<vg::MultipathAlignment>(out, records.size(),
          [&records](size_t i) { return records.at(i); }, compressed), "fixture write failed");
    vg::io::finish(out, compressed);
    return out.str();
}

// A real VG stream can contain other registered protobuf types and empty tagged
// groups. The serial MultipathAlignment iterator ignores both; retain that as
// the differential oracle instead of encoding the filtering policy here.
std::string encode_with_non_gamp_frames(const std::vector<vg::MultipathAlignment>& records) {
    std::ostringstream out(std::ios::binary);
    std::vector<vg::Alignment> wrong_type(1);
    wrong_type.front().set_name("not-a-gamp");
    check(vg::io::write<vg::Alignment>(out, wrong_type.size(),
          [&wrong_type](size_t i) { return wrong_type.at(i); }, false), "wrong-type write failed");
    check(vg::io::write<vg::Alignment>(out, 0,
          [](size_t) { return vg::Alignment(); }, false), "tag-only write failed");
    check(vg::io::write<vg::MultipathAlignment>(out, records.size(),
          [&records](size_t i) { return records.at(i); }, false), "GAMP write failed");
    vg::io::finish(out, false);
    return out.str();
}

std::string encode_bad_protobuf_after_prefix(const vg::MultipathAlignment& good) {
    std::ostringstream out(std::ios::binary);
    {
        vg::io::MessageEmitter emitter(out, false, 1);
        const auto tag = vg::io::Registry::get_protobuf_tag<vg::MultipathAlignment>();
        emitter.write_copy(tag, good.SerializeAsString());
        emitter.write(tag, std::string("\x80", 1)); // Unterminated protobuf field varint.
    }
    return out.str();
}

std::string encode_bad_protobuf_before_records(const std::vector<vg::MultipathAlignment>& records) {
    std::ostringstream out(std::ios::binary);
    {
        vg::io::MessageEmitter emitter(out, false, 1);
        const auto tag = vg::io::Registry::get_protobuf_tag<vg::MultipathAlignment>();
        emitter.write(tag, std::string("\x80", 1));
        for (const auto& record : records) emitter.write_copy(tag, record.SerializeAsString());
    }
    return out.str();
}

std::vector<std::string> serial(const std::string& bytes) {
    std::istringstream in(bytes, std::ios::binary);
    std::vector<std::string> observed;
    vg::io::for_each<vg::MultipathAlignment>(in, [&](vg::MultipathAlignment& record) {
        observed.push_back(record.SerializeAsString());
    });
    return observed;
}

class NonSeekableBuffer : public std::stringbuf {
public:
    explicit NonSeekableBuffer(const std::string& bytes) : std::stringbuf(bytes, std::ios::in | std::ios::binary) {}
protected:
    pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) override {
        return pos_type(off_type(-1));
    }
    pos_type seekpos(pos_type, std::ios_base::openmode) override { return pos_type(off_type(-1)); }
};

std::vector<std::string> ordered(const std::string& bytes, bool nonseekable, size_t threads,
                                 pancollapse::input::OrderedGampReaderStats* stats = nullptr,
                                 std::function<void(uint64_t)> before_decode = {}) {
    pancollapse::input::OrderedGampReaderOptions options;
    options.decoder_threads = threads;
    options.decompression_threads = 2;
    options.batch_records = 2;
    options.batch_bytes = 32;
    options.max_inflight_batches = 2;
    options.max_buffer_bytes = 1024 * 1024;
    options.before_decode = std::move(before_decode);
    std::vector<std::string> observed;
    if (nonseekable) {
        NonSeekableBuffer buffer(bytes);
        std::istream in(&buffer);
        auto result = pancollapse::input::ordered_gamp_for_each(in, [&](vg::MultipathAlignment& record) {
            observed.push_back(record.SerializeAsString());
        }, options);
        if (stats) *stats = result;
    } else {
        std::istringstream in(bytes, std::ios::binary);
        auto result = pancollapse::input::ordered_gamp_for_each(in, [&](vg::MultipathAlignment& record) {
            observed.push_back(record.SerializeAsString());
        }, options);
        if (stats) *stats = result;
    }
    return observed;
}

void run_differential(const std::string& bytes, bool nonseekable) {
    const auto expected = serial(bytes);
    for (const size_t threads : {size_t(0), size_t(1), size_t(2), size_t(4), size_t(8)}) {
        pancollapse::input::OrderedGampReaderStats stats;
        const auto observed = ordered(bytes, nonseekable, threads, &stats);
        check(observed == expected, "ordered delivery differs from serial vg reader");
        check(stats.records == expected.size(), "record statistic differs from delivered records");
        check(stats.batches >= 1, "no batches recorded");
        check(stats.peak_buffer_bytes <= 1024 * 1024 + stats.raw_bytes,
              "buffer statistic exceeds bounded allowance");
    }
}

void run_cancellation(const std::string& bytes) {
    pancollapse::input::OrderedGampReaderOptions options;
    options.decoder_threads = 4;
    options.batch_records = 1;
    options.batch_bytes = 1;
    options.max_inflight_batches = 2;
    options.max_buffer_bytes = 1024;
    std::istringstream in(bytes, std::ios::binary);
    bool threw = false;
    try {
        pancollapse::input::ordered_gamp_for_each(in, [](vg::MultipathAlignment&) {
            throw std::runtime_error("callback cancellation");
        }, options);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "callback cancellation";
    }
    check(threw, "callback exception was not propagated");
}

void run_slow_earliest(const std::string& bytes) {
    const auto expected = serial(bytes);
    const auto observed = ordered(bytes, false, 4, nullptr, [](uint64_t ordinal) {
        if (ordinal == 0) std::this_thread::sleep_for(std::chrono::milliseconds(30));
    });
    check(observed == expected, "slow first batch reordered or deadlocked delivery");
}

void run_oversize_record() {
    vg::MultipathAlignment record;
    record.set_name("oversize");
    record.set_sequence(std::string(64U * 1024U * 1024U + 1U, 'A'));
    const auto bytes = encode({record}, true);
    pancollapse::input::OrderedGampReaderStats stats;
    const auto observed = ordered(bytes, false, 2, &stats);
    check(observed.size() == 1 && observed.front() == record.SerializeAsString(),
          "over-64MiB record differs from serializable input");
    check(stats.budget_oversize_records == 1, "over-budget record was not accounted as an exception");
    check(stats.raw_bytes > 64U * 1024U * 1024U && stats.decoded_bytes > 64U * 1024U * 1024U,
          "raw/decoded statistics omit oversized payload");
    check(stats.peak_raw_bytes > 1024U * 1024U && stats.peak_decoded_bytes > 1024U * 1024U &&
              stats.peak_buffer_bytes >= stats.peak_raw_bytes,
          "oversized peak-buffer statistics are implausible");
}

void run_corruption(const std::string& bytes) {
    std::string truncated = bytes.substr(0, bytes.size() - 3);
    std::istringstream in(truncated, std::ios::binary);
    std::vector<std::string> prefix;
    bool threw = false;
    try {
        pancollapse::input::OrderedGampReaderOptions options;
        options.decoder_threads = 4;
        options.batch_records = 1;
        options.batch_bytes = 1;
        pancollapse::input::ordered_gamp_for_each(in, [&](vg::MultipathAlignment& record) {
            prefix.push_back(record.SerializeAsString());
        }, options);
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "truncated GAMP did not fail");
    const auto expected = serial(bytes);
    check(prefix.size() <= expected.size() &&
              std::equal(prefix.begin(), prefix.end(), expected.begin()),
          "corrupt input delivered a reordered prefix");
}

struct FailureOutcome {
    std::vector<std::string> prefix;
    std::string error;
    bool operator==(const FailureOutcome&) const = default;
};

template<class Reader>
FailureOutcome capture_failure(Reader&& reader) {
    FailureOutcome result;
    try {
        reader([&](vg::MultipathAlignment& record) {
            result.prefix.push_back(record.SerializeAsString());
        });
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

void run_error_differential(const std::string& bytes) {
    const auto serial_result = capture_failure([&](const auto& callback) {
        std::istringstream in(bytes, std::ios::binary);
        vg::io::for_each<vg::MultipathAlignment>(in, callback);
    });
    const auto ordered_result = capture_failure([&](const auto& callback) {
        pancollapse::input::OrderedGampReaderOptions options;
        options.decoder_threads = 4;
        options.batch_records = 1;
        options.batch_bytes = 1;
        options.max_inflight_batches = 2;
        options.max_buffer_bytes = 1024;
        std::istringstream in(bytes, std::ios::binary);
        pancollapse::input::ordered_gamp_for_each(in, callback, options);
    });
    check(!serial_result.error.empty() && serial_result == ordered_result,
          "ordered reader error or delivered prefix differs from serial vg reader");
}

void run_group_recurrence_differential() {
    auto records = fixture();
    records.push_back(records.front()); // A after B: an adjacent-QNAME consumer must reject it.
    const auto bytes = encode(records, false);
    const auto validate_groups = [](const auto& run) {
        return capture_failure([&](const auto& callback) {
            std::string current;
            std::vector<std::string> complete;
            run([&](vg::MultipathAlignment& record) {
                if (!current.empty() && record.name() != current) {
                    complete.push_back(current);
                    current = record.name();
                } else if (current.empty()) {
                    current = record.name();
                }
                if (std::find(complete.begin(), complete.end(), record.name()) != complete.end()) {
                    throw std::runtime_error("recurrent group");
                }
                callback(record);
            });
        });
    };
    const auto serial_result = validate_groups([&](const auto& callback) {
        std::istringstream in(bytes, std::ios::binary);
        vg::io::for_each<vg::MultipathAlignment>(in, callback);
    });
    const auto ordered_result = validate_groups([&](const auto& callback) {
        pancollapse::input::OrderedGampReaderOptions options;
        options.decoder_threads = 4;
        options.batch_records = 2;
        options.batch_bytes = 32;
        std::istringstream in(bytes, std::ios::binary);
        pancollapse::input::ordered_gamp_for_each(in, callback, options);
    });
    check(serial_result == ordered_result && serial_result.error == "recurrent group",
          "QNAME recurrence callback differs from serial delivery");
}

template<class Run>
FailureOutcome callback_failure_outcome(Run&& run) {
    FailureOutcome result;
    try {
        run([&](vg::MultipathAlignment& record) {
            result.prefix.push_back(record.SerializeAsString());
            throw std::runtime_error("first callback failure");
        });
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

void run_admission_failure_regressions() {
    std::vector<vg::MultipathAlignment> records;
    for (size_t i = 0; i < 10; ++i) {
        vg::MultipathAlignment record;
        record.set_name("admission" + std::to_string(i));
        record.set_sequence(std::string(128, 'A'));
        records.push_back(std::move(record));
    }
    const auto bytes = encode(records, false);
    const auto serial_result = callback_failure_outcome([&](const auto& callback) {
        std::istringstream in(bytes, std::ios::binary);
        vg::io::for_each<vg::MultipathAlignment>(in, callback);
    });
    const auto ordered_result = [&](size_t inflight, size_t budget) {
        return callback_failure_outcome([&](const auto& callback) {
            pancollapse::input::OrderedGampReaderOptions options;
            options.decoder_threads = 2;
            options.batch_records = 1;
            options.batch_bytes = 1;
            options.max_inflight_batches = inflight;
            options.max_buffer_bytes = budget;
            std::istringstream in(bytes, std::ios::binary);
            pancollapse::input::ordered_gamp_for_each(in, callback, options);
        });
    };
    check(ordered_result(2, 1U << 20) == serial_result,
          "slot-admission callback failure delivered records after serial failure");
    check(ordered_result(8, 800) == serial_result,
          "byte-admission callback failure delivered records after serial failure");

    const auto malformed = encode_bad_protobuf_before_records(records);
    const auto serial_parse = capture_failure([&](const auto& callback) {
        std::istringstream in(malformed, std::ios::binary);
        vg::io::for_each<vg::MultipathAlignment>(in, callback);
    });
    const auto ordered_parse = capture_failure([&](const auto& callback) {
        pancollapse::input::OrderedGampReaderOptions options;
        options.decoder_threads = 2;
        options.batch_records = 1;
        options.batch_bytes = 1;
        options.max_inflight_batches = 2;
        options.max_buffer_bytes = 1U << 20;
        std::istringstream in(malformed, std::ios::binary);
        pancollapse::input::ordered_gamp_for_each(in, callback, options);
    });
    check(ordered_parse == serial_parse && ordered_parse.prefix.empty(),
          "admission-time parse failure delivered records after serial failure");
}

}  // namespace

int main() {
    try {
        const auto records = fixture();
        const auto plain = encode(records, false);
        const auto bgzf = encode(records, true);
        const auto mixed = encode_with_non_gamp_frames(records);
        run_differential(plain, false);
        run_differential(plain, true);
        run_differential(bgzf, false);
        run_differential(bgzf, true);
        run_differential(mixed, false);
        run_slow_earliest(plain);
        run_cancellation(plain);
        run_corruption(plain);
        run_error_differential(plain.substr(0, plain.size() - 3));
        run_error_differential(encode_bad_protobuf_after_prefix(records.front()));
        run_group_recurrence_differential();
        run_admission_failure_regressions();
        run_oversize_record();
        std::cout << "PASS: ordered GAMP reader differential, stream, BGZF, filtering, oversized, cancellation, and error-prefix checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
