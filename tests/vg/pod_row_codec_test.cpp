#include "pod_row_codec.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <zstd.h>

namespace {

using pancollapse::pod_row_codec::Encoder;
using pancollapse::pod_row_codec::Encoding;

struct TripleRow {
    std::uint64_t id = 0;
    std::uint32_t flags = 0;
    std::uint32_t support = 0;

    bool operator==(const TripleRow&) const = default;
};
static_assert(sizeof(TripleRow) == 16);
static_assert(std::is_trivially_copyable_v<TripleRow>);

void require(bool value, std::string_view context) {
    if (!value) throw std::runtime_error(std::string(context));
}

template<class F>
void expect_rejection(F&& action, std::string_view context, std::string_view expected) {
    try {
        action();
    } catch (const std::exception& error) {
        if (std::string_view(error.what()).find(expected) != std::string_view::npos) return;
        throw std::runtime_error(std::string(context) + ": unexpected diagnostic: " + error.what());
    }
    throw std::runtime_error(std::string(context) + ": expected rejection");
}

bool same_bytes(std::span<const std::byte> left, std::span<const std::byte> right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

template<class T>
std::span<const std::byte> bytes_of(const std::vector<T>& values) {
    return std::as_bytes(std::span<const T>(values.data(), values.size()));
}

template<class T, std::size_t N>
std::span<const std::byte> bytes_of(const std::array<T, N>& values) {
    return std::as_bytes(std::span<const T, N>(values));
}

std::vector<TripleRow> compressed_rows() {
    std::vector<TripleRow> rows;
    rows.reserve(4096);
    for (std::uint32_t index = 0; index != 4096; ++index) {
        rows.push_back({0xf123456789abc000ULL + index % 23,
                        std::uint32_t{0x80000000U} | (index % 8),
                        std::uint32_t{0x40000000U} | ((index * 3) % 16)});
    }
    return rows;
}

std::vector<std::uint64_t> seeded_incompressible_rows() {
    std::vector<std::uint64_t> rows(4096);
    std::uint64_t state = 0x9e3779b97f4a7c15ULL;
    for (auto& row : rows) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        row = state * 0x2545f4914f6cdd1dULL;
    }
    return rows;
}

struct ContextDeleter {
    void operator()(ZSTD_CCtx* context) const noexcept { ZSTD_freeCCtx(context); }
};

struct LegacyEncoded {
    std::vector<std::byte> bytes;
    Encoding encoding = Encoding::raw;
};

LegacyEncoded legacy_encode(ZSTD_CCtx* context, std::vector<std::byte>& scratch,
                            std::span<const std::byte> input) {
    if (input.empty()) return {};
    scratch.resize(ZSTD_compressBound(input.size()));
    const auto size = ZSTD_compressCCtx(context, scratch.data(), scratch.size(), input.data(),
                                        input.size(), 3);
    if (ZSTD_isError(size)) throw std::runtime_error("legacy level-3 zstd compression failed");
    LegacyEncoded result;
    if (size >= input.size()) {
        result.bytes.assign(input.begin(), input.end());
        return result;
    }
    scratch.resize(size);
    result.bytes = scratch;
    result.encoding = Encoding::zstd;
    return result;
}

void expect_legacy_level3_match(Encoder& encoder, ZSTD_CCtx* legacy_context,
                                std::vector<std::byte>& legacy_scratch,
                                std::span<const std::byte> input, std::string_view context) {
    const auto helper = encoder.encode(input, 3);
    const auto legacy = legacy_encode(legacy_context, legacy_scratch, input);
    require(helper.encoding == legacy.encoding, std::string(context) + ": encoding selection");
    require(same_bytes(helper.bytes, legacy.bytes), std::string(context) + ": selected payload bytes");
}

}  // namespace

int main() try {
    const auto rows = compressed_rows();
    const auto row_bytes = bytes_of(rows);

    Encoder encoder;
    const auto empty = encoder.encode({}, 3);
    require(empty.encoding == Encoding::raw && empty.bytes.empty(), "empty input uses raw fallback");
    require(pancollapse::pod_row_codec::decode<std::uint32_t>(empty.bytes, empty.encoding, 0).empty(),
            "empty raw decode has zero rows");

    const std::array<TripleRow, 1> small{{{0xf000000000000001ULL, 0x80000005U, 0x40000009U}}};
    const auto small_encoded = encoder.encode(bytes_of(small), 3);
    require(small_encoded.encoding == Encoding::raw, "small POD input uses raw fallback");
    const auto small_decoded = pancollapse::pod_row_codec::decode<TripleRow>(
        small_encoded.bytes, small_encoded.encoding, small.size());
    require(small_decoded.size() == small.size() && small_decoded.front() == small.front(),
            "small raw POD roundtrip");

    const auto large_encoded = encoder.encode(row_bytes, 3);
    require(large_encoded.encoding == Encoding::zstd, "large repetitive POD input uses zstd");
    const std::vector<std::byte> compressed(large_encoded.bytes.begin(), large_encoded.bytes.end());
    const auto decoded = pancollapse::pod_row_codec::decode<TripleRow>(
        compressed, large_encoded.encoding, rows.size());
    require(decoded.size() == rows.size() && decoded == rows, "compressed POD exact row count and values");
    require(decoded.front().id > (std::uint64_t{1} << 63) &&
                decoded[17].flags == rows[17].flags && decoded[17].support == rows[17].support &&
                (decoded[17].flags & 0x80000000U) != 0 &&
                (decoded[17].support & 0x40000000U) != 0,
            "high IDs and flags/support bits survive zstd roundtrip");

    const auto incompressible = seeded_incompressible_rows();
    const auto incompressible_encoded = encoder.encode(bytes_of(incompressible), 3);
    require(incompressible_encoded.encoding == Encoding::raw, "seeded incompressible input uses raw fallback");
    require(pancollapse::pod_row_codec::decode<std::uint64_t>(
                incompressible_encoded.bytes, incompressible_encoded.encoding, incompressible.size()) == incompressible,
            "seeded incompressible raw roundtrip");

    const std::array<std::byte, 1> one_byte{{std::byte{0xa5}}};
    expect_rejection(
        [] {
            static_cast<void>(pancollapse::pod_row_codec::decode<TripleRow>(
                {}, Encoding::raw, std::numeric_limits<std::size_t>::max()));
        },
        "overflow count", "POD row decode size overflows");
    expect_rejection(
        [&] {
            static_cast<void>(pancollapse::pod_row_codec::decode<TripleRow>(one_byte, Encoding::raw, 1));
        },
        "wrong raw length", "raw POD row size differs from expected output");
    expect_rejection(
        [] {
            static_cast<void>(pancollapse::pod_row_codec::decode<TripleRow>({}, Encoding::zstd, 1));
        },
        "empty compressed input", "compressed POD row size is invalid");
    require(compressed.size() > 1, "compressed fixture has a truncatable frame");
    expect_rejection(
        [&] {
            static_cast<void>(pancollapse::pod_row_codec::decode<TripleRow>(
                {compressed.data(), compressed.size() - 1}, Encoding::zstd, rows.size()));
        },
        "truncated compressed frame", "compressed POD row payload differs from expected output");
    auto corrupt = compressed;
    corrupt.front() ^= std::byte{0xff};
    expect_rejection(
        [&] {
            static_cast<void>(pancollapse::pod_row_codec::decode<TripleRow>(corrupt, Encoding::zstd, rows.size()));
        },
        "corrupt compressed frame", "compressed POD row payload differs from expected output");
    expect_rejection(
        [&] {
            static_cast<void>(pancollapse::pod_row_codec::decode<TripleRow>(
                compressed, Encoding::zstd, rows.size() + 1));
        },
        "wrong zstd output size", "compressed POD row payload differs from expected output");
    expect_rejection(
        [] {
            static_cast<void>(pancollapse::pod_row_codec::decode<std::uint32_t>(
                {}, static_cast<Encoding>(255), 0));
        },
        "unknown encoding", "unknown POD row encoding");

    Encoder compatibility_encoder;
    std::unique_ptr<ZSTD_CCtx, ContextDeleter> legacy_context(ZSTD_createCCtx());
    require(static_cast<bool>(legacy_context), "legacy zstd context allocation");
    std::vector<std::byte> legacy_scratch;
    const std::vector<std::byte> repetitive(8192, std::byte{0x5a});
    expect_legacy_level3_match(compatibility_encoder, legacy_context.get(), legacy_scratch, {},
                               "legacy empty raw input");
    expect_legacy_level3_match(compatibility_encoder, legacy_context.get(), legacy_scratch,
                               bytes_of(small), "legacy small raw POD input");
    expect_legacy_level3_match(compatibility_encoder, legacy_context.get(), legacy_scratch,
                               repetitive, "legacy compressible input");
    expect_legacy_level3_match(compatibility_encoder, legacy_context.get(), legacy_scratch, row_bytes,
                               "legacy large POD input");
    expect_legacy_level3_match(compatibility_encoder, legacy_context.get(), legacy_scratch,
                               bytes_of(incompressible), "legacy seeded raw input");

    Encoder trimmable;
    const auto trim_encoded = trimmable.encode(row_bytes, 3);
    require(trim_encoded.encoding == Encoding::zstd, "trim source uses zstd");
    const std::vector<std::byte> copied_before_trim(trim_encoded.bytes.begin(), trim_encoded.bytes.end());
    const auto trim_encoding = trim_encoded.encoding;
    const auto retained_before_trim = trimmable.retained_bytes();
    require(retained_before_trim != 0, "encoder reports retained context and scratch bytes");
    trimmable.release_buffers_over(std::numeric_limits<std::size_t>::max());
    require(trimmable.retained_bytes() == retained_before_trim, "encoder normally retains reusable buffers");
    trimmable.release_buffers_over(0);
    require(trimmable.retained_bytes() == 0, "trim zero releases context and scratch");
    require(pancollapse::pod_row_codec::decode<TripleRow>(
                copied_before_trim, trim_encoding, rows.size()) == rows,
            "copied compressed view roundtrips after trim");
    const auto after_trim = trimmable.encode(row_bytes, 3);
    require(after_trim.encoding == Encoding::zstd, "encoder is reusable after trim");
    constexpr std::size_t native_cap = 4 * 1024 * 1024;
    trimmable.release_buffers_over(native_cap);
    require(trimmable.retained_bytes() <= native_cap, "retained encoder bytes honor cap");

    std::cout << "pod row codec checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "pod row codec test failure: " << error.what() << '\n';
    return 1;
}
