#pragma once

// PathTally step 1: reproduce vg's per-node alignment score.
//
// vg computes a subpath's score as a left-to-right sum over the subpath's
// mappings (nodes) and edits (see vg src/alignment_scorer.cpp
// MatrixAlignmentScorer::score_partial_alignment; defaults in
// alignment_scorer.hpp). Because the score is additive over nodes we reproduce
// it per node here. Summed over a subpath, these per-node scores must equal the
// stored Subpath.score -- that equality is the acceptance test for step 1.

#include <vg/vg.pb.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pathtally {

// vg default alignment scoring parameters (alignment_scorer.hpp:18-28).
struct ScoreParams {
    int64_t match = 1;
    int64_t mismatch = 4;
    int64_t gap_open = 6;
    int64_t gap_extension = 1;
    int64_t full_length_bonus = 5;
};

// Score one subpath's path, attributing each mapping's (node's) contribution to
// per_node[i]. read_start_offset is the absolute read position where this
// subpath's read segment begins; read_total_length is the whole read length
// (both used only for full-length-bonus / soft-clip detection). When
// no_read_end_scoring is true the read-end bonus is not added and read-end
// insertions (soft clips) are penalized like ordinary insertions.
inline int64_t score_subpath(const vg::Subpath& subpath,
                             size_t read_start_offset,
                             size_t read_total_length,
                             bool no_read_end_scoring,
                             const ScoreParams& params,
                             std::vector<int64_t>* per_node) {
    const vg::Path& path = subpath.path();
    if (per_node != nullptr) {
        per_node->assign(static_cast<size_t>(path.mapping_size()), 0);
    }

    int64_t total = 0;
    size_t read_pos = read_start_offset;
    bool in_deletion = false;

    for (int i = 0; i < path.mapping_size(); ++i) {
        const vg::Mapping& mapping = path.mapping(i);
        int64_t node_score = 0;
        for (int j = 0; j < mapping.edit_size(); ++j) {
            const vg::Edit& edit = mapping.edit(j);
            const int64_t from_length = edit.from_length();
            const int64_t to_length = edit.to_length();

            if (from_length > 0) {
                if (to_length > 0) {
                    if (edit.sequence().empty()) {
                        node_score += params.match * from_length;
                    } else {
                        node_score -= params.mismatch * from_length;
                    }
                    if (!no_read_end_scoring) {
                        if (read_pos == 0) {
                            node_score += params.full_length_bonus;
                        }
                        if (read_pos + static_cast<size_t>(to_length) == read_total_length) {
                            node_score += params.full_length_bonus;
                        }
                    }
                    in_deletion = false;
                } else if (in_deletion) {
                    node_score -= from_length * params.gap_extension;
                } else {
                    node_score -= params.gap_open + (from_length - 1) * params.gap_extension;
                    in_deletion = true;
                }
            } else if (to_length > 0) {
                const bool at_read_end =
                    (read_pos == 0) || (read_pos + static_cast<size_t>(to_length) == read_total_length);
                if (no_read_end_scoring || !at_read_end) {
                    node_score -= params.gap_open + (to_length - 1) * params.gap_extension;
                }
                in_deletion = false;
            }

            read_pos += static_cast<size_t>(to_length);
        }
        total += node_score;
        if (per_node != nullptr) {
            (*per_node)[static_cast<size_t>(i)] = node_score;
        }
    }

    return total;
}

// Read length (in read bases) consumed by one subpath.
inline size_t subpath_read_length(const vg::Subpath& subpath) {
    size_t length = 0;
    for (const vg::Mapping& mapping : subpath.path().mapping()) {
        for (const vg::Edit& edit : mapping.edit()) {
            length += static_cast<size_t>(edit.to_length());
        }
    }
    return length;
}

// Absolute read start offset for every subpath, propagated from the read-start
// subpaths through next and connection edges. Subpaths are stored in
// topological order (vg.proto Subpath comment), so a single forward pass fills
// every reachable subpath. Unreachable subpaths keep offset 0.
inline std::vector<size_t> subpath_read_offsets(const vg::MultipathAlignment& alignment) {
    const int count = alignment.subpath_size();
    std::vector<size_t> offset(static_cast<size_t>(count), 0);
    std::vector<char> known(static_cast<size_t>(count), 0);

    if (alignment.start_size() > 0) {
        for (const uint32_t start : alignment.start()) {
            if (start < static_cast<uint32_t>(count)) {
                offset[start] = 0;
                known[start] = 1;
            }
        }
    } else {
        std::vector<char> has_incoming(static_cast<size_t>(count), 0);
        for (int i = 0; i < count; ++i) {
            for (const uint32_t next : alignment.subpath(i).next()) {
                if (next < static_cast<uint32_t>(count)) has_incoming[next] = 1;
            }
            for (const vg::Connection& c : alignment.subpath(i).connection()) {
                if (c.next() < static_cast<uint32_t>(count)) has_incoming[c.next()] = 1;
            }
        }
        for (int i = 0; i < count; ++i) {
            if (!has_incoming[static_cast<size_t>(i)]) known[static_cast<size_t>(i)] = 1;
        }
    }

    for (int i = 0; i < count; ++i) {
        if (!known[static_cast<size_t>(i)]) continue;
        const size_t child_offset =
            offset[static_cast<size_t>(i)] + subpath_read_length(alignment.subpath(i));
        for (const uint32_t next : alignment.subpath(i).next()) {
            if (next < static_cast<uint32_t>(count) && !known[next]) {
                offset[next] = child_offset;
                known[next] = 1;
            }
        }
        for (const vg::Connection& c : alignment.subpath(i).connection()) {
            if (c.next() < static_cast<uint32_t>(count) && !known[c.next()]) {
                offset[c.next()] = child_offset;
                known[c.next()] = 1;
            }
        }
    }

    return offset;
}

}  // namespace pathtally
