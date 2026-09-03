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
#include <optional>
#include <set>
#include <stdexcept>
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
// Production ledgers already assign every relevant graph path a stable numeric handle. Keeping
// that handle through the hot tally avoids hashing and copying long vg path names per read; names
// are recovered only for winning BAM provenance.
using NumericTallyMap = absl::flat_hash_map<uint64_t, HstTally>;

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
    // Populated by the explicit path-identity selector. Legacy t2g callers leave
    // these empty so their historical result surface is unchanged.
    std::vector<std::string> winning_paths;
    std::vector<std::string> winning_parents;
};

struct ResolvedPathIdentity {
    std::string unique_parent;
    std::string canonical_transcript;
};

struct NumericPathIdentity {
    std::string unique_parent;
    uint32_t canonical_target_id = 0;
};

struct RankedPathIdentity {
    const std::string* path_name = nullptr;
    const std::string* unique_parent = nullptr;
    uint32_t parent_rank = 0;
    uint32_t canonical_target_id = 0;
    bool is_exon = false;
};

// Compact production-only collapse result. Pointer vectors can be accumulated without allocating
// one ordered-tree node per winning path/Parent. The caller lexically sorts them only if that
// target/layer is ultimately emitted.
struct CompactCollapsedIdentityTally {
    int64_t score = std::numeric_limits<int64_t>::min();
    int64_t forward_bases = 0;
    int64_t reverse_bases = 0;
    std::vector<const std::string*> winning_paths;
    std::vector<const std::string*> winning_parents;
};

struct CompactIdentityLayers {
    std::map<uint32_t, CompactCollapsedIdentityTally> exon;
    std::map<uint32_t, CompactCollapsedIdentityTally> body;
};

template <class IdentityResolver>
inline CompactIdentityLayers collapse_ranked_identity_tallies(
    const NumericTallyMap& tallies, IdentityResolver&& resolve_identity) {
    struct ParentEvidence {
        uint32_t target_id = 0;
        const std::string* unique_parent = nullptr;
        bool is_exon = false;
        int64_t score = std::numeric_limits<int64_t>::min();
        int64_t forward_bases = 0;
        int64_t reverse_bases = 0;
        const std::string* first_winning_path = nullptr;
        std::vector<const std::string*> additional_winning_paths;
    };
    // A layer bit keeps exon/body evidence independent without reserving two
    // full-size flat hash tables for every read group. Parent ranks are only
    // 32 bits, so the combined key remains collision-free.
    using ParentMap = absl::flat_hash_map<uint64_t, ParentEvidence>;
    ParentMap parents;
    parents.reserve(tallies.size());

    for (const auto& [path_handle, tally] : tallies) {
        const std::optional<RankedPathIdentity> identity = resolve_identity(path_handle);
        if (!identity.has_value()) {
            continue;
        }
        const uint64_t layered_parent_key =
            (static_cast<uint64_t>(identity->parent_rank) << 1U) |
            static_cast<uint64_t>(identity->is_exon);
        auto [it, inserted] = parents.try_emplace(layered_parent_key);
        if (inserted) {
            it->second.target_id = identity->canonical_target_id;
            it->second.unique_parent = identity->unique_parent;
            it->second.is_exon = identity->is_exon;
        }
        if (!inserted && it->second.target_id != identity->canonical_target_id) {
            throw std::runtime_error("unique Parent resolves to multiple canonical targets");
        }
        ParentEvidence& evidence = it->second;
        if (inserted || tally.score > evidence.score) {
            evidence.score = tally.score;
            evidence.forward_bases = tally.forward_bases;
            evidence.reverse_bases = tally.reverse_bases;
            evidence.first_winning_path = identity->path_name;
            evidence.additional_winning_paths.clear();
        } else if (tally.score == evidence.score) {
            evidence.forward_bases += tally.forward_bases;
            evidence.reverse_bases += tally.reverse_bases;
            evidence.additional_winning_paths.push_back(identity->path_name);
        }
    }

    CompactIdentityLayers result;
    for (const auto& [layered_parent_key, parent] : parents) {
        static_cast<void>(layered_parent_key);
        auto& targets = parent.is_exon ? result.exon : result.body;
        CompactCollapsedIdentityTally& target = targets[parent.target_id];
        if (parent.score > target.score) {
            target.score = parent.score;
            target.forward_bases = parent.forward_bases;
            target.reverse_bases = parent.reverse_bases;
            target.winning_paths.clear();
            target.winning_paths.reserve(
                1 + parent.additional_winning_paths.size());
            target.winning_paths.push_back(parent.first_winning_path);
            target.winning_paths.insert(
                target.winning_paths.end(),
                parent.additional_winning_paths.begin(),
                parent.additional_winning_paths.end());
            target.winning_parents.assign(1, parent.unique_parent);
        } else if (parent.score == target.score) {
            target.forward_bases += parent.forward_bases;
            target.reverse_bases += parent.reverse_bases;
            target.winning_paths.push_back(parent.first_winning_path);
            target.winning_paths.insert(
                target.winning_paths.end(),
                parent.additional_winning_paths.begin(),
                parent.additional_winning_paths.end());
            target.winning_parents.push_back(parent.unique_parent);
        }
    }

    return result;
}

struct CollapsedIdentityTally {
    int64_t score = std::numeric_limits<int64_t>::min();
    int64_t forward_bases = 0;
    int64_t reverse_bases = 0;
    std::set<std::string> winning_paths;
    std::set<std::string> winning_parents;
};

inline void retain_max_path_evidence(CollapsedIdentityTally& evidence,
                                     const std::string& path_name,
                                     const std::string& unique_parent,
                                     const HstTally& tally) {
    if (tally.score > evidence.score) {
        evidence.score = tally.score;
        evidence.forward_bases = tally.forward_bases;
        evidence.reverse_bases = tally.reverse_bases;
        evidence.winning_paths = {path_name};
        evidence.winning_parents = {unique_parent};
    } else if (tally.score == evidence.score) {
        evidence.forward_bases += tally.forward_bases;
        evidence.reverse_bases += tally.reverse_bases;
        evidence.winning_paths.insert(path_name);
        evidence.winning_parents.insert(unique_parent);
    }
}

inline void retain_max_parent_evidence(CollapsedIdentityTally& evidence,
                                       const std::string& unique_parent,
                                       const CollapsedIdentityTally& parent) {
    if (parent.score > evidence.score) {
        evidence = parent;
        evidence.winning_parents = {unique_parent};
    } else if (parent.score == evidence.score) {
        evidence.forward_bases += parent.forward_bases;
        evidence.reverse_bases += parent.reverse_bases;
        evidence.winning_paths.insert(parent.winning_paths.begin(), parent.winning_paths.end());
        evidence.winning_parents.insert(unique_parent);
    }
}

// Production identity collapse for one layer. Every exact path is resolved first,
// paths MAX-collapse within unique_parent, then Parents MAX-collapse within canonical
// transcript target. Missing paths are ignored so the caller can run this independently
// for exon and body layers. Scores are never summed; all tied score-winning paths and
// Parents are retained in sorted sets for BAM provenance.
template <class IdentityResolver>
inline std::map<uint32_t, CollapsedIdentityTally> collapse_identity_tallies(
    const TallyMap& tallies, IdentityResolver&& resolve_identity) {
    struct ParentEvidence {
        uint32_t target_id = 0;
        CollapsedIdentityTally evidence;
    };
    std::map<std::string, ParentEvidence> per_parent;
    for (const auto& [path_name, tally] : tallies) {
        const std::optional<NumericPathIdentity> identity = resolve_identity(path_name);
        if (!identity.has_value()) {
            continue;
        }
        auto [it, inserted] = per_parent.try_emplace(
            identity->unique_parent, ParentEvidence{identity->canonical_target_id, {}});
        if (!inserted && it->second.target_id != identity->canonical_target_id) {
            throw std::runtime_error("unique Parent resolves to multiple canonical targets");
        }
        retain_max_path_evidence(it->second.evidence, path_name, identity->unique_parent, tally);
    }

    std::map<uint32_t, CollapsedIdentityTally> per_target;
    for (const auto& [unique_parent, parent] : per_parent) {
        retain_max_parent_evidence(per_target[parent.target_id], unique_parent, parent.evidence);
    }
    return per_target;
}

// Production score-mode selector. The explicit path -> Parent -> canonical MAX
// hierarchy is observable through winning_paths/winning_parents. Only canonical
// targets tied at the global top score are emitted, preserving D048 semantics.
template <class IdentityResolver>
inline std::vector<RadTarget> select_identity_targets(const TallyMap& tallies,
                                                      IdentityResolver&& resolve_identity) {
    struct ParentEvidence {
        std::string canonical_transcript;
        CollapsedIdentityTally evidence;
    };
    std::map<std::string, ParentEvidence> per_parent;
    for (const auto& [path_name, tally] : tallies) {
        const ResolvedPathIdentity identity = resolve_identity(path_name);
        auto [it, inserted] = per_parent.try_emplace(
            identity.unique_parent, ParentEvidence{identity.canonical_transcript, {}});
        if (!inserted && it->second.canonical_transcript != identity.canonical_transcript) {
            throw std::runtime_error("unique Parent resolves to multiple canonical transcripts");
        }
        retain_max_path_evidence(it->second.evidence, path_name, identity.unique_parent, tally);
    }

    std::map<std::string, CollapsedIdentityTally> per_transcript;
    for (const auto& [unique_parent, parent] : per_parent) {
        retain_max_parent_evidence(per_transcript[parent.canonical_transcript], unique_parent,
                                   parent.evidence);
    }
    int64_t top = std::numeric_limits<int64_t>::min();
    for (const auto& [transcript, evidence] : per_transcript) {
        top = std::max(top, evidence.score);
    }

    std::vector<RadTarget> targets;
    for (const auto& [transcript, evidence] : per_transcript) {
        if (evidence.score != top) {
            continue;
        }
        targets.push_back({transcript,
                           evidence.forward_bases >= evidence.reverse_bases,
                           {evidence.winning_paths.begin(), evidence.winning_paths.end()},
                           {evidence.winning_parents.begin(), evidence.winning_parents.end()}});
    }
    return targets;
}

// Steps 3-4 (select): winners are the HSTs tied at the single top score; collapse
// them to unique transcript IDs; per transcript, orientation is the majority of
// aligned bases (forward on an exact tie); targets sorted by transcript id. The raw
// path top score is selected BEFORE transcript collapse. This order is essential when
// several graph paths alias to one transcript: their scores are alternatives and must
// never be summed to manufacture a winning transcript.
template <class TranscriptResolver>
inline std::vector<RadTarget> select_targets(const TallyMap& tallies,
                                             TranscriptResolver&& resolve_transcript) {
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
        auto& evidence = per_transcript[resolve_transcript(name)];
        evidence.first += tally.forward_bases;
        evidence.second += tally.reverse_bases;
    }

    for (const auto& [transcript, evidence] : per_transcript) {
        targets.push_back({transcript, evidence.first >= evidence.second, {}, {}});
    }
    return targets;
}

// Backward-compatible two-column t2g behavior: derive transcript identity from the
// conventional trailing haplotype-copy suffix.
inline std::vector<RadTarget> select_targets(const TallyMap& tallies) {
    return select_targets(tallies, [](const std::string& name) { return transcript_id_of(name); });
}

}  // namespace pathtally
