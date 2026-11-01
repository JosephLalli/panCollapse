#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <vg/vg.pb.h>

namespace pancollapse::prepared_count {
namespace detail {

inline std::uint64_t signature_size(std::size_t value) {
    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
        if (value > static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max())) {
            throw std::overflow_error("prepared count assignment signature size overflows uint64");
        }
    }
    return static_cast<std::uint64_t>(value);
}

inline void append_assignment_signature_integer(std::string& output, std::uint64_t value) {
    while (value >= 0x80U) {
        output.push_back(static_cast<char>((value & 0x7fU) | 0x80U));
        value >>= 7;
    }
    output.push_back(static_cast<char>(value));
}

inline std::size_t assignment_signature_varint_size(std::uint64_t value) {
    return value == 0 ? 1 : (std::bit_width(value) + 6) / 7;
}

inline void add_assignment_signature_bytes(std::size_t& size, std::size_t bytes) {
    if (bytes > std::numeric_limits<std::size_t>::max() - size) {
        throw std::overflow_error("prepared count assignment signature size overflows size_t");
    }
    size += bytes;
}

template<class Sink>
inline void visit_assignment_signature_fields(
    const std::vector<const vg::MultipathAlignment*>& records, Sink&& sink) {
    sink(signature_size(records.size()));
    for (const vg::MultipathAlignment* alignment : records) {
        if (alignment == nullptr) {
            throw std::runtime_error("prepared count received null alignment");
        }
        sink(signature_size(alignment->sequence().size()));
        sink(signature_size(alignment->start_size()));
        for (const auto start : alignment->start()) {
            sink(static_cast<std::uint64_t>(static_cast<std::int64_t>(start)));
        }
        sink(signature_size(alignment->subpath_size()));
        for (const vg::Subpath& subpath : alignment->subpath()) {
            const vg::Path& path = subpath.path();
            sink(signature_size(path.mapping_size()));
            for (const vg::Mapping& mapping : path.mapping()) {
                const vg::Position& position = mapping.position();
                sink(static_cast<std::uint64_t>(position.node_id()));
                sink(static_cast<std::uint64_t>(position.offset()));
                sink(position.is_reverse());
                sink(signature_size(mapping.edit_size()));
                for (const vg::Edit& edit : mapping.edit()) {
                    sink(static_cast<std::uint64_t>(edit.from_length()));
                    sink(static_cast<std::uint64_t>(edit.to_length()));
                    sink(!edit.sequence().empty());
                }
            }
            sink(signature_size(subpath.next_size()));
            for (const auto next : subpath.next()) sink(next);
            sink(signature_size(subpath.connection_size()));
            for (const vg::Connection& connection : subpath.connection()) sink(connection.next());
        }
    }
}

// Reserve exactly the canonical encoding below. The same visitor supplies the
// size-counting and encoding passes, so their framing cannot diverge.
inline std::size_t assignment_signature_capacity(
    const std::vector<const vg::MultipathAlignment*>& records) {
    std::size_t size = 0;
    visit_assignment_signature_fields(records, [&](std::uint64_t value) {
        add_assignment_signature_bytes(size, assignment_signature_varint_size(value));
    });
    return size;
}

}  // namespace detail

// Exact graph-evidence key for a read group. Deliberately excludes QNAME,
// sequence content, and quality: those are molecule-specific and are handled
// after assignment. It encodes exactly the graph evidence consumed by the
// node-score resolver, with canonical unsigned-varint fields and explicit repeated-field
// counts to preserve ordering and boundaries.
inline std::string assignment_graph_signature(
    const std::vector<const vg::MultipathAlignment*>& records) {
    std::string output;
    output.reserve(detail::assignment_signature_capacity(records));
    detail::visit_assignment_signature_fields(records, [&](std::uint64_t value) {
        detail::append_assignment_signature_integer(output, value);
    });
    return output;
}

}  // namespace pancollapse::prepared_count
