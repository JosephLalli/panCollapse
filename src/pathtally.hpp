#pragma once

// PathTally steps 2-4: attribute per-node scores to the HST paths crossing each
// node, select the winning HSTs (top pooled score across a read's alignments plus
// ties), collapse them to unique transcript IDs, and record per-transcript
// orientation by majority of aligned bases.
//
// The graph lookup ("which HST paths cross this node, and in which orientation")
// is injected as a PathLookup so this logic is testable without a graph; the
// production path implements it with xg for_each_step_on_handle.

#include "pathtally_score.hpp"

#include <vg/vg.pb.h>

#include <absl/container/flat_hash_map.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace pathtally {

struct HstTally {
    int64_t score = 0;
    int64_t forward_bases = 0;
    int64_t reverse_bases = 0;
};

// Per-read-group tally, keyed by HST path name. A flat (open-addressing) hash map
// rather than std::map: the tally is written once per node-visit (the hot loop), so
// its cost dominated the run as red-black-tree string comparisons (memcmp). The flat
// map hashes instead of comparing, and callers reuse one instance across groups
// (clear() keeps the backing storage) so per-group node allocation disappears. Order
// does not matter: accumulation is commutative and select_targets re-derives the top
// score and re-sorts.
using TallyMap = absl::flat_hash_map<std::string, HstTally>;

// For a graph node id, emit(path_name, path_is_reverse) once per HST path step on
// that node.
using PathLookup =
    std::function<void(int64_t node_id, const std::function<void(const std::string&, bool)>&)>;

// Aligned (reference- and read-consuming) bases on one mapping.
inline int64_t mapping_aligned_bases(const vg::Mapping& mapping) {
    int64_t bases = 0;
    for (const vg::Edit& edit : mapping.edit()) {
        if (edit.from_length() > 0 && edit.to_length() > 0) {
            bases += edit.from_length();
        }
    }
    return bases;
}

// Strip a trailing _H<digits> or _R<digits> haplotype suffix to get the
// transcript id. Names without that suffix are returned unchanged.
inline std::string transcript_id_of(const std::string& hst_name) {
    const size_t underscore = hst_name.rfind('_');
    if (underscore == std::string::npos || underscore + 2 >= hst_name.size()) {
        return hst_name;
    }
    const char tag = hst_name[underscore + 1];
    if (tag != 'H' && tag != 'R') {
        return hst_name;
    }
    for (size_t k = underscore + 2; k < hst_name.size(); ++k) {
        if (std::isdigit(static_cast<unsigned char>(hst_name[k])) == 0) {
            return hst_name;
        }
    }
    return hst_name.substr(0, underscore);
}

// Per-subpath node scorer: fill per_node with the vg score of each node in
// mp.subpath(subpath_index), whose read segment begins at read_offset.
using NodeScorer = std::function<void(const vg::MultipathAlignment& mp, int subpath_index,
                                      size_t read_offset, std::vector<int64_t>& per_node)>;

// Default scorer: the flat (non-quality-adjusted) reproduction of vg's scheme.
inline NodeScorer flat_scorer(const ScoreParams& params = ScoreParams{}) {
    return [params](const vg::MultipathAlignment& mp, int subpath_index, size_t read_offset,
                    std::vector<int64_t>& per_node) {
        score_subpath(mp.subpath(subpath_index), read_offset, mp.sequence().size(), false, params, &per_node);
    };
}

// Steps 2-3 (tally), reused-workspace form: clears `tallies` (keeping its backing
// storage) and refills it. Across every alignment of one read, add each node's vg
// score to every HST path crossing it, and accumulate orientation evidence (aligned
// bases forward vs reverse relative to the path). Callers that process many groups
// pass one long-lived TallyMap here so per-group allocation is amortized away.
inline void tally_read_group_into(
    TallyMap& tallies,
    const std::vector<const vg::MultipathAlignment*>& records,
    const PathLookup& lookup,
    const NodeScorer& node_scorer = flat_scorer()) {
    tallies.clear();
    std::vector<int64_t> per_node;

    for (const vg::MultipathAlignment* record : records) {
        const vg::MultipathAlignment& mp = *record;
        const std::vector<size_t> offsets = subpath_read_offsets(mp);

        for (int s = 0; s < mp.subpath_size(); ++s) {
            const vg::Subpath& subpath = mp.subpath(s);
            node_scorer(mp, s, offsets[static_cast<size_t>(s)], per_node);
            const vg::Path& path = subpath.path();
            for (int i = 0; i < path.mapping_size(); ++i) {
                const vg::Mapping& mapping = path.mapping(i);
                const int64_t node_id = mapping.position().node_id();
                const bool read_is_reverse = mapping.position().is_reverse();
                const int64_t node_score = per_node[static_cast<size_t>(i)];
                const int64_t aligned_bases = mapping_aligned_bases(mapping);

                lookup(node_id, [&](const std::string& path_name, bool path_is_reverse) {
                    HstTally& tally = tallies[path_name];
                    tally.score += node_score;
                    if (read_is_reverse == path_is_reverse) {
                        tally.forward_bases += aligned_bases;
                    } else {
                        tally.reverse_bases += aligned_bases;
                    }
                });
            }
        }
    }
}

// Convenience form that allocates a fresh TallyMap. Prefer tally_read_group_into with
// a reused map on hot paths that process many groups.
inline TallyMap tally_read_group(
    const std::vector<const vg::MultipathAlignment*>& records,
    const PathLookup& lookup,
    const NodeScorer& node_scorer = flat_scorer()) {
    TallyMap tallies;
    tally_read_group_into(tallies, records, lookup, node_scorer);
    return tallies;
}

struct RadTarget {
    std::string transcript;
    bool forward = true;
};

// Steps 3-4 (select): winners are the HSTs tied at the single top score; collapse
// them to unique transcript IDs; per transcript, orientation is the majority of
// aligned bases (forward on an exact tie); targets sorted by transcript id.
inline std::vector<RadTarget> select_targets(const TallyMap& tallies) {
    std::vector<RadTarget> targets;
    if (tallies.empty()) {
        return targets;
    }

    int64_t top = std::numeric_limits<int64_t>::min();
    for (const auto& [name, tally] : tallies) {
        top = std::max(top, tally.score);
    }

    // std::map keeps transcript ids sorted, which is the RAD dictionary rule.
    std::map<std::string, std::pair<int64_t, int64_t>> per_transcript;  // forward, reverse bases
    for (const auto& [name, tally] : tallies) {
        if (tally.score != top) {
            continue;
        }
        auto& evidence = per_transcript[transcript_id_of(name)];
        evidence.first += tally.forward_bases;
        evidence.second += tally.reverse_bases;
    }

    for (const auto& [transcript, evidence] : per_transcript) {
        targets.push_back({transcript, evidence.first >= evidence.second});
    }
    return targets;
}

}  // namespace pathtally
