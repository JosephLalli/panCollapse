#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <zstd.h>

namespace pancollapse::pod_row_codec {

enum class Encoding : std::uint8_t { raw, zstd };

// A raw view is valid while input lives. A zstd view is valid until another
// encode() overwrites the scratch or release_buffers_over() releases it.
struct EncodedView { std::span<const std::byte> bytes; Encoding encoding = Encoding::raw; };

class Encoder {
  public:
    EncodedView encode(std::span<const std::byte> input, int level) {
        if (input.empty()) return {{}, Encoding::raw};
        if (!context_) {
            context_.reset(ZSTD_createCCtx());
            if (!context_) throw std::bad_alloc();
        }
        scratch_.resize(ZSTD_compressBound(input.size()));
        const auto size = ZSTD_compressCCtx(context_.get(), scratch_.data(), scratch_.size(),
                                            input.data(), input.size(), level);
        if (ZSTD_isError(size)) throw std::runtime_error(ZSTD_getErrorName(size));
        if (size >= input.size()) return {input, Encoding::raw};
        scratch_.resize(size);
        return {std::span<const std::byte>(scratch_.data(), scratch_.size()), Encoding::zstd};
    }

    [[nodiscard]] std::size_t retained_bytes() const noexcept {
        const auto scratch_bytes = scratch_.capacity();
        if (!context_) return scratch_bytes;
        const auto context_bytes = ZSTD_sizeof_CCtx(context_.get());
        if (context_bytes > std::numeric_limits<std::size_t>::max() - scratch_bytes) {
            return std::numeric_limits<std::size_t>::max();
        }
        return context_bytes + scratch_bytes;
    }

    // Invalidates zstd EncodedViews when it releases the retained workspace.
    void release_buffers_over(std::size_t maximum_retained_bytes) {
        if (retained_bytes() <= maximum_retained_bytes) return;
        context_.reset();
        std::vector<std::byte>{}.swap(scratch_);
    }

  private:
    struct ContextDeleter { void operator()(ZSTD_CCtx* value) const noexcept { ZSTD_freeCCtx(value); } };
    std::unique_ptr<ZSTD_CCtx, ContextDeleter> context_;
    std::vector<std::byte> scratch_;
};

template<class T>
std::vector<T> decode(std::span<const std::byte> input, Encoding encoding, std::size_t count) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T))
        throw std::overflow_error("POD row decode size overflows");
    const auto bytes = count * sizeof(T);
    switch (encoding) {
        case Encoding::raw:
            if (input.size() != bytes) throw std::runtime_error("raw POD row size differs from expected output");
            break;
        case Encoding::zstd:
            if (bytes == 0 || input.empty()) throw std::runtime_error("compressed POD row size is invalid");
            break;
        default:
            throw std::runtime_error("unknown POD row encoding");
    }

    std::vector<T> result(count);
    if (encoding == Encoding::raw) {
        if (bytes != 0) std::memcpy(result.data(), input.data(), bytes);
        return result;
    }

    const auto actual = ZSTD_decompress(result.data(), bytes, input.data(), input.size());
    if (ZSTD_isError(actual) || actual != bytes)
        throw std::runtime_error("compressed POD row payload differs from expected output");
    return result;
}

}  // namespace pancollapse::pod_row_codec
