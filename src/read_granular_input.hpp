#pragma once

#include <vg/io/message_iterator.hpp>
#include <vg/io/protobuf_iterator.hpp>
#include <vg/io/registry.hpp>
#include <vg/vg.pb.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <istream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pancollapse::input {

// What the producer needs from a serialized MultipathAlignment to group reads without
// decoding it: the read name and whether any subpath is present. `name` views the
// serialized bytes it was peeked from.
struct MultipathRecordPeek {
    std::string_view name;
    bool has_subpath = false;
};

namespace detail {

inline bool read_wire_varint(const unsigned char*& cursor, const unsigned char* end,
                             std::uint64_t& value) {
    value = 0;
    for (int shift = 0; shift < 64 && cursor < end; shift += 7) {
        const unsigned char byte = *cursor++;
        value |= static_cast<std::uint64_t>(byte & 0x7FU) << shift;
        if ((byte & 0x80U) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace detail

// Scan only the top-level fields of a serialized vg::MultipathAlignment. Each subpath is
// one length-delimited field skipped in constant time, so the cost is independent of how
// many mappings the alignment carries. Proto3 semantics are reproduced: the last `name`
// wins, and an empty embedded subpath still counts as a subpath. Returns false for any
// encoding this scan does not model (groups, a known field under an unexpected wire type,
// truncation); the caller then decodes the record in full, which stays exact.
inline bool peek_multipath_record(std::string_view bytes, MultipathRecordPeek& peek) {
    constexpr std::uint64_t kNameField = 3;     // vg.proto MultipathAlignment.name
    constexpr std::uint64_t kSubpathField = 6;  // vg.proto MultipathAlignment.subpath
    peek = {};
    const auto* cursor = reinterpret_cast<const unsigned char*>(bytes.data());
    const auto* const end = cursor + bytes.size();
    while (cursor < end) {
        std::uint64_t tag = 0;
        if (!detail::read_wire_varint(cursor, end, tag)) {
            return false;
        }
        const std::uint64_t field = tag >> 3;
        const std::uint64_t wire_type = tag & 7U;
        if (field == 0) {
            return false;
        }
        switch (wire_type) {
        case 0: {
            if (field == kNameField || field == kSubpathField) {
                return false;
            }
            std::uint64_t ignored = 0;
            if (!detail::read_wire_varint(cursor, end, ignored)) {
                return false;
            }
            break;
        }
        case 1:
        case 5: {
            const std::size_t width = wire_type == 1 ? 8 : 4;
            if (field == kNameField || field == kSubpathField ||
                static_cast<std::size_t>(end - cursor) < width) {
                return false;
            }
            cursor += width;
            break;
        }
        case 2: {
            std::uint64_t length = 0;
            if (!detail::read_wire_varint(cursor, end, length) ||
                length > static_cast<std::uint64_t>(end - cursor)) {
                return false;
            }
            if (field == kNameField) {
                peek.name = std::string_view(reinterpret_cast<const char*>(cursor),
                                             static_cast<std::size_t>(length));
            } else if (field == kSubpathField) {
                peek.has_subpath = true;
            }
            cursor += length;
            break;
        }
        default:
            return false;
        }
    }
    return true;
}

// Decode one serialized record exactly as the vg reader does.
inline void decode_multipath_record(const std::string& bytes, vg::MultipathAlignment& record) {
    if (!vg::io::ProtobufIterator<vg::MultipathAlignment>::parse_from_string(record, bytes)) {
        throw std::runtime_error("[io::ProtobufIterator] could not parse message");
    }
}

struct ReadGranularInputStats {
    std::uint64_t records = 0;
    std::uint64_t raw_bytes = 0;
    // Wall clock on the framing thread, partitioned: pulling and framing raw records,
    // versus the callback (grouping, and waiting for queue admission).
    double producer_input_seconds = 0.0;
    double producer_callback_seconds = 0.0;
};

// Frame a GAMP stream and hand each serialized MultipathAlignment to the callback, in
// input order, on the calling thread. Nothing is decoded here: the callback owns the bytes.
template<class Callback>
ReadGranularInputStats raw_gamp_for_each(std::istream& input, Callback&& callback,
                                         std::size_t decompression_threads) {
    static_assert(std::is_invocable_v<Callback&, std::unique_ptr<std::string>>,
                  "raw GAMP callback must accept std::unique_ptr<std::string>");
    ReadGranularInputStats stats;
    vg::io::MessageIterator message_iterator(input, false, decompression_threads);
    std::exception_ptr reader_failure;
    const auto input_started = std::chrono::steady_clock::now();
    while (message_iterator.has_current()) {
        auto& tagged = *message_iterator;
        const bool right_tag =
            vg::io::Registry::check_protobuf_tag<vg::MultipathAlignment>(tagged.first);
        if (right_tag && tagged.second) {
            stats.raw_bytes += tagged.second->size();
            ++stats.records;
            const auto callback_started = std::chrono::steady_clock::now();
            std::invoke(callback, std::move(tagged.second));
            stats.producer_callback_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() - callback_started).count();
        }
        try {
            // A framing failure follows the record already handed over, as in the
            // ordered reader.
            message_iterator.advance();
        } catch (...) {
            reader_failure = std::current_exception();
            break;
        }
    }
    stats.producer_input_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - input_started).count()
        - stats.producer_callback_seconds;
    if (reader_failure) {
        std::rethrow_exception(reader_failure);
    }
    return stats;
}

}  // namespace pancollapse::input
