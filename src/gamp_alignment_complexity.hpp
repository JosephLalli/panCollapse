#pragma once

#include <vg/vg.pb.h>

#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace pancollapse::input {

inline constexpr std::uint64_t kDefaultMaxAlignmentsPerRead = 64000000;

// Counts reported source-to-sink routes only far enough to decide whether a
// QNAME's records exceed limit. The reusable vectors avoid per-read allocation.
class GampAlignmentComplexity {
public:
    bool exceeds(std::span<const vg::MultipathAlignment* const> records,
                 std::uint64_t limit) {
        if (limit == 0) return false;
        if (limit == std::numeric_limits<std::uint64_t>::max()) {
            throw std::invalid_argument("GAMP alignment-complexity limit must be below UINT64_MAX");
        }
        const std::uint64_t cap = limit + 1;
        std::uint64_t total = 0;
        for (const vg::MultipathAlignment* record : records) {
            if (record == nullptr) throw std::invalid_argument("null GAMP alignment record");
            const int subpath_count = record->subpath_size();
            if (subpath_count == 0) continue;
            const size_t count = static_cast<size_t>(subpath_count);
            routes_.assign(count, 0);
            incoming_.assign(count, 0);
            for (size_t i = 0; i < count; ++i) {
                const vg::Subpath& subpath = record->subpath(static_cast<int>(i));
                validate_edges(subpath.next(), i, count);
                for (const vg::Connection& connection : subpath.connection()) {
                    validate_edge(connection.next(), i, count);
                }
            }
            for (size_t i = count; i-- > 0;) {
                const vg::Subpath& subpath = record->subpath(static_cast<int>(i));
                bool has_next = false;
                std::uint64_t routes = 0;
                for (const std::uint32_t next : subpath.next()) {
                    has_next = true;
                    routes = capped_add(routes, routes_[next], cap);
                }
                for (const vg::Connection& connection : subpath.connection()) {
                    has_next = true;
                    routes = capped_add(routes, routes_[connection.next()], cap);
                }
                routes_[i] = has_next ? routes : 1;
            }
            if (record->start_size() > 0) {
                for (const std::uint32_t start : record->start()) {
                    if (start >= count) throw std::invalid_argument("GAMP start index is out of range");
                    total = capped_add(total, routes_[start], cap);
                    if (total == cap) return true;
                }
            } else {
                for (size_t i = 0; i < count; ++i) {
                    if (!incoming_[i]) {
                        total = capped_add(total, routes_[i], cap);
                        if (total == cap) return true;
                    }
                }
            }
        }
        return false;
    }

private:
    static std::uint64_t capped_add(std::uint64_t a, std::uint64_t b, std::uint64_t cap) {
        return a >= cap - b ? cap : a + b;
    }

    void validate_edges(const google::protobuf::RepeatedField<std::uint32_t>& edges,
                        size_t source, size_t count) {
        for (const std::uint32_t target : edges) validate_edge(target, source, count);
    }

    void validate_edge(std::uint32_t target, size_t source, size_t count) {
        if (target >= count) throw std::invalid_argument("GAMP edge target is out of range");
        if (target <= source) throw std::invalid_argument("GAMP subpaths are not topologically ordered");
        incoming_[target] = 1;
    }

    std::vector<std::uint64_t> routes_;
    std::vector<std::uint8_t> incoming_;
};

}  // namespace pancollapse::input
