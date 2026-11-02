#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace pancollapse::model_set {

struct Block {
    std::uint32_t id = 0;
    std::array<std::uint64_t, 4> words{};
};
using Set = std::vector<Block>;

inline bool empty(const Block& block) {
    return !(block.words[0] | block.words[1] | block.words[2] | block.words[3]);
}
inline void append(Set& set, std::uint32_t id) {
    if (set.empty() || set.back().id != (id >> 8)) set.push_back({id >> 8, {}});
    set.back().words[(id & 255) >> 6] |= std::uint64_t{1} << (id & 63);
}
template<class Visit> void each(const Set& set, Visit&& visit) {
    for (const auto& block : set) for (unsigned word = 0; word < 4; ++word) {
        auto bits = block.words[word];
        while (bits) {
            visit((block.id << 8) | (word << 6) | std::countr_zero(bits));
            bits &= bits - 1;
        }
    }
}
inline bool contains(const Set& set, std::uint32_t id) {
    const auto found = std::lower_bound(set.begin(), set.end(), id >> 8,
        [](const auto& block, auto key) { return block.id < key; });
    return found != set.end() && found->id == (id >> 8) &&
        (found->words[(id & 255) >> 6] & (std::uint64_t{1} << (id & 63)));
}
// Outputs must not alias inputs. Scratch keeps capacity, never stale logical rows.
// Sorted block operations never construct one object per transcript membership.
inline void unite(Set& a, const Set& b, Set& scratch) {
    assert(&scratch != &a && &scratch != &b);
    scratch.clear();
    scratch.reserve(a.size() + b.size());
    std::size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        if (i == a.size() || (j < b.size() && b[j].id < a[i].id)) {
            scratch.push_back(b[j++]);
        } else if (j == b.size() || a[i].id < b[j].id) {
            scratch.push_back(a[i++]);
        } else {
            Block block{a[i].id, {}};
            for (unsigned w = 0; w < 4; ++w)
                block.words[w] = a[i].words[w] | b[j].words[w];
            scratch.push_back(block);
            ++i; ++j;
        }
    }
    a.swap(scratch);
}
// Produce a & b and a & ~b in one monotone merge; retain both output capacities.
inline void split(const Set& a, const Set& b, Set& yes, Set& no) {
    assert(&yes != &a && &yes != &b && &no != &a && &no != &b && &yes != &no);
    yes.clear(); no.clear();
    yes.reserve(std::min(a.size(), b.size()));
    no.reserve(a.size());
    std::size_t j = 0;
    for (const auto& block : a) {
        while (j < b.size() && b[j].id < block.id) ++j;
        if (j == b.size() || b[j].id != block.id) {
            no.push_back(block);
        } else {
            Block intersection{block.id, {}}, difference{block.id, {}};
            for (unsigned w = 0; w < 4; ++w) {
                intersection.words[w] = block.words[w] & b[j].words[w];
                difference.words[w] = block.words[w] & ~b[j].words[w];
            }
            if (!empty(intersection)) yes.push_back(intersection);
            if (!empty(difference)) no.push_back(difference);
        }
    }
}

inline void put32(std::vector<std::byte>& out, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<std::byte>(value >> (i * 8)));
}
inline std::uint32_t get32(std::span<const std::byte> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4)
        throw std::runtime_error("truncated model-set header");
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i)
        result |= std::uint32_t(std::to_integer<unsigned char>(data[offset + i])) << (8 * i);
    return result;
}
// Each 256-ID block chooses a low-byte array, inclusive runs, or a bitmap.
// Directory entries are twelve bytes and payload offsets are relative to the set.
inline std::vector<std::byte> encode(const Set& set) {
    std::vector<std::byte> directory, payload;
    std::uint64_t cardinality = 0;
    for (const auto& block : set) {
        std::vector<std::byte> array, runs;
        for (unsigned bit = 0; bit < 256; ++bit)
            if (block.words[bit >> 6] & (std::uint64_t{1} << (bit & 63)))
                array.push_back(static_cast<std::byte>(bit));
        if (array.empty()) throw std::runtime_error("empty model-set block");
        for (std::size_t i = 0; i < array.size();) {
            const auto start = array[i];
            auto last = start;
            while (++i < array.size() && std::to_integer<unsigned>(array[i]) ==
                std::to_integer<unsigned>(last) + 1) last = array[i];
            runs.push_back(start); runs.push_back(last);
        }
        unsigned kind = 0;
        if (runs.size() < array.size()) kind = 1;
        if (32 < (kind == 0 ? array.size() : runs.size())) kind = 2;
        const auto offset = 8ULL + set.size() * 12ULL + payload.size();
        if (offset > UINT32_MAX) throw std::runtime_error("model-set payload too large");
        put32(directory, block.id); put32(directory, static_cast<std::uint32_t>(offset));
        directory.push_back(static_cast<std::byte>(array.size() - 1));
        directory.push_back(static_cast<std::byte>(kind));
        directory.push_back(std::byte{0}); directory.push_back(std::byte{0});
        if (kind < 2) {
            const auto& selected = kind == 0 ? array : runs;
            payload.insert(payload.end(), selected.begin(), selected.end());
        } else for (const auto word : block.words) for (unsigned i = 0; i < 8; ++i)
            payload.push_back(static_cast<std::byte>(word >> (i * 8)));
        cardinality += array.size();
    }
    if (cardinality > UINT32_MAX || set.size() > UINT32_MAX)
        throw std::runtime_error("model-set cardinality overflow");
    std::vector<std::byte> result;
    put32(result, static_cast<std::uint32_t>(set.size()));
    put32(result, static_cast<std::uint32_t>(cardinality));
    result.insert(result.end(), directory.begin(), directory.end());
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
inline Set decode(std::span<const std::byte> bytes, std::size_t model_count) {
    const auto blocks = get32(bytes, 0), cardinality = get32(bytes, 4);
    if (blocks > (bytes.size() - 8) / 12) throw std::runtime_error("model-set directory range");
    Set result;
    result.reserve(blocks);
    std::size_t expected = 8 + std::size_t(blocks) * 12, seen = 0;
    for (std::size_t i = 0; i < blocks; ++i) {
        const auto at = 8 + i * 12;
        Block block{get32(bytes, at), {}};
        const auto offset = get32(bytes, at + 4);
        const auto count = std::to_integer<unsigned>(bytes[at + 8]) + 1;
        const auto kind = std::to_integer<unsigned>(bytes[at + 9]);
        const auto end = i + 1 == blocks ? bytes.size() : get32(bytes, at + 16);
        if (offset != expected || end < offset || end > bytes.size() || kind > 2 ||
            bytes[at + 10] != std::byte{0} || bytes[at + 11] != std::byte{0} ||
            (i && block.id <= result.back().id) || block.id > (UINT32_MAX >> 8))
            throw std::runtime_error("invalid model-set block");
        const auto mark = [&](unsigned low) {
            block.words[low >> 6] |= std::uint64_t{1} << (low & 63);
        };
        if (kind == 0) {
            if (end - offset != count) throw std::runtime_error("model-set array size");
            unsigned prior = 0;
            for (std::size_t p = offset; p < end; ++p) {
                const auto low = std::to_integer<unsigned>(bytes[p]);
                if (p != offset && low <= prior) throw std::runtime_error("model-set array order");
                mark(low); prior = low;
            }
        } else if (kind == 1) {
            if ((end - offset) % 2) throw std::runtime_error("model-set run size");
            unsigned prior = 0;
            for (std::size_t p = offset; p < end; p += 2) {
                const auto start = std::to_integer<unsigned>(bytes[p]);
                const auto last = std::to_integer<unsigned>(bytes[p + 1]);
                if (start > last || (p != offset && start <= prior + 1))
                    throw std::runtime_error("model-set run order");
                for (unsigned low = start; low <= last; ++low) mark(low);
                prior = last;
            }
        } else {
            if (end - offset != 32) throw std::runtime_error("model-set bitmap size");
            for (unsigned w = 0; w < 4; ++w) for (unsigned b = 0; b < 8; ++b)
                block.words[w] |= std::uint64_t(std::to_integer<unsigned char>(bytes[offset + w * 8 + b])) << (8 * b);
        }
        unsigned actual = 0;
        for (const auto word : block.words) actual += std::popcount(word);
        if (actual != count) throw std::runtime_error("model-set cardinality mismatch");
        for (unsigned w = 0; w < 4; ++w)
            if (block.words[w] && ((std::uint64_t(block.id) << 8) | (w << 6) |
                (63 - std::countl_zero(block.words[w]))) >= model_count)
                throw std::runtime_error("model-set model range");
        seen += actual; result.push_back(block); expected = end;
    }
    if (expected != bytes.size() || seen != cardinality) throw std::runtime_error("model-set framing");
    return result;
}

struct FlagSet { std::uint32_t flags = 0; Set models; };
inline bool overlaps(const Set& a, const Set& b) {
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].id < b[j].id) ++i;
        else if (b[j].id < a[i].id) ++j;
        else {
            for (unsigned w = 0; w < 4; ++w) if (a[i].words[w] & b[j].words[w]) return true;
            ++i; ++j;
        }
    }
    return false;
}
inline std::vector<FlagSet> decode_flags(std::span<const std::byte> bytes, std::size_t models,
                                        std::uint32_t expected_count = UINT32_MAX) {
    std::vector<FlagSet> result;
    std::size_t offset = 0;
    std::uint64_t count = 0;
    while (offset < bytes.size()) {
        const auto flags = get32(bytes, offset), size = get32(bytes, offset + 4);
        offset += 8;
        if (!flags || flags > 15 || (!result.empty() && flags <= result.back().flags) ||
            size > bytes.size() - offset) throw std::runtime_error("flag-set framing");
        auto decoded = decode(bytes.subspan(offset, size), models);
        if (decoded.empty()) throw std::runtime_error("empty flag set");
        for (const auto& prior : result) if (overlaps(prior.models, decoded))
            throw std::runtime_error("overlapping flag sets");
        count += get32(bytes, offset + 4);
        result.push_back({flags, std::move(decoded)});
        offset += size;
    }
    if (expected_count != UINT32_MAX && count != expected_count)
        throw std::runtime_error("flag-set count mismatch");
    return result;
}
}  // namespace pancollapse::model_set
