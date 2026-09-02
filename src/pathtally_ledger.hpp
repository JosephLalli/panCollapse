#pragma once

// D060: per-transcript intron-touch classification for the ledger count modes (gene / genefull /
// genefull_exonoverintron / genefull_ex50pas).
//
// panCollapse itself no longer summarizes a gene's compatibility -- it classifies every candidate
// TRANSCRIPT the read is compatible with as spliced (exon-only) or unspliced (intron-touching), and
// leaves gene grouping, ambiguity (a gene with both a spliced and an unspliced transcript), the
// count-mode rule, and the sense/antisense policy to the downstream counter (count_cr). This file
// is the one piece of that classification that is pure and graph-free, so it is unit-tested
// directly; main.cpp does the per-node vg-score tally (reusing pathtally::tally_read_group_into,
// the SAME machinery --count-mode score uses) and the node-id-range bookkeeping, collapses each
// transcript's/gene's raw haplotype-copy paths down to the maps below, and calls classify_ledger_group.
//
// Transcript and gene identity are opaque uint32_t indices here (a RAD/BAM transcript target id and
// an internal gene index, respectively) -- main.cpp owns the string<->index mapping (T2gData); this
// file only needs to group a gene's transcripts and dedup by transcript id.
//
// D061: splice-junction concordance tightens the S (spliced) call. Node-membership alone (a
// transcript's exon path ties the read's top score) is too loose: a read can land its bases on
// transcript A's exon while the node-skip it actually makes is a DIFFERENT transcript B's intron (a
// graph-sharing coincidence, e.g. tail-to-tail overlapping genes). classify_ledger_group now also
// requires a transcript to OWN every splice edge the read crosses -- STAR's classifyAlign
// concordance gate (a spliced read's donor/acceptor must equal one of the transcript's own introns),
// kept strand-blind exactly as STAR does (orientation is applied later by count_cr via GD).
// splice_concordant_transcripts computes the read's concordant set from the raw node-id pairs its
// alignment crosses (main.cpp extracts these from the MultipathAlignment and looks them up against
// the SpliceEdgeMap it builds at graph load, alongside transcript_spans); this file stays graph-free
// by taking those pairs as plain int64_t data.

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pathtally {

// --count-mode selector. Score is the default non-ledger D048 path (handled entirely in main.cpp
// via tally_read_group_into/select_targets). Gene, GeneFull, and GeneFullExonOverIntron drive the
// same S/U transcript classification and let count_cr apply their different policies downstream.
// Production GeneFullEx50pAS is the D066 exception: main.cpp emits exact Parent-preserving E/P/B
// overlap evidence in TX/GX/GD/GT/XP/XU. The historical hst-v1 adapter cannot represent that
// evidence and is rejected for this mode.
enum class CountMode { Score, Gene, GeneFull, GeneFullExonOverIntron, GeneFullEx50pAS };

// STARsolo 2.7.11b Transcriptome_alignExonOverlap.cpp reduces each individual
// alignment/transcript comparison to one of three strand-neutral tiers before orientation expands
// them to the six ordered Ex50pAS overlap types:
//   E: every reference-aligned base overlaps an exon and every splice junction is concordant;
//   P: strictly more than half of the reference-aligned bases overlap exons;
//   B: the alignment is contained in the transcript body but does not meet E or P.
//
// Keep this arithmetic free of integer division: STAR uses nOverlap > exl/2, which for an odd exl
// is equivalent to exon_overlap > body_overlap. Exactly half is B. A fully exonic but
// splice-discordant alignment deliberately falls through to P.
enum class Ex50Tier : char { FullyExonic = 'E', ExonicMajority = 'P', Body = 'B' };

inline Ex50Tier classify_ex50_tier(uint64_t exon_overlap, uint64_t alignment_bases,
                                  bool splice_concordant) {
    if (alignment_bases == 0 || exon_overlap > alignment_bases) {
        throw std::invalid_argument("invalid base totals for Ex50pAS tier classification");
    }
    if (exon_overlap == alignment_bases && splice_concordant) {
        return Ex50Tier::FullyExonic;
    }
    if (exon_overlap > alignment_bases - exon_overlap) {
        return Ex50Tier::ExonicMajority;
    }
    return Ex50Tier::Body;
}

inline char ex50_tier_code(Ex50Tier tier) {
    return static_cast<char>(tier);
}

// One exon transcript's precomputed on-body exon node-id span (graph-load-time, main.cpp): the
// transcript "spans" a read iff [lo, hi] brackets the read's touched gene-body node-id range.
struct TranscriptSpan {
    int64_t lo = 0;
    int64_t hi = 0;
    uint32_t transcript_target_id = 0;
};

// One classified transcript: its RAD/BAM target id and spliced (true=S) / unspliced (false=U) flag.
struct LedgerCall {
    uint32_t transcript_target_id = 0;
    bool spliced = false;
};

// D063 transcript-body ledger classification. Both score maps are keyed by the SAME canonical
// transcript target id: exon_score is max-collapsed across its raw spliced paths and body_score
// across its raw unspliced body paths. The global raw-reference top score is shared across both
// maps. A near-top exon score calls S; otherwise a near-top body score for that exact transcript
// calls U. Spliced wins when both layers tie. The D061 splice-concordance gate applies equally to
// S and U, so a read that makes a splice the transcript does not own is absent rather than
// spuriously demoted to unspliced. No gene identity enters this function; transcript-to-gene
// pooling belongs to the downstream counter.
inline std::vector<LedgerCall> classify_transcript_body_ledger_group(
    const std::map<uint32_t, int64_t>& exon_score,
    const std::map<uint32_t, int64_t>& body_score, int64_t flank,
    const std::optional<std::set<uint32_t>>& splice_concordant = std::nullopt) {
    int64_t top = std::numeric_limits<int64_t>::min();
    for (const auto& [transcript, score] : exon_score) {
        top = std::max(top, score);
    }
    for (const auto& [transcript, score] : body_score) {
        top = std::max(top, score);
    }
    if (top == std::numeric_limits<int64_t>::min()) {
        return {};
    }

    auto is_concordant = [&](uint32_t transcript) {
        return !splice_concordant.has_value() || splice_concordant->count(transcript) != 0;
    };

    std::map<uint32_t, bool> emitted;  // transcript target id -> spliced, sorted + deduped
    for (const auto& [transcript, score] : exon_score) {
        if (score >= top - flank && is_concordant(transcript)) {
            emitted[transcript] = true;
        }
    }
    for (const auto& [transcript, score] : body_score) {
        if (score >= top - flank && is_concordant(transcript)) {
            emitted.emplace(transcript, false);  // no-op if the exon layer already called S
        }
    }

    std::vector<LedgerCall> calls;
    calls.reserve(emitted.size());
    for (const auto& [transcript, spliced] : emitted) {
        calls.push_back({transcript, spliced});
    }
    return calls;
}

// D061: global undirected splice-edge ownership map, built once at graph load (main.cpp, alongside
// transcript_spans). Key is an on-body node-id pair (lo,hi) with lo<hi that some exon transcript's
// own path treats as a splice (its consecutive steps skip over real gene-body sequence there, i.e.
// an intron); value is the set of transcript target ids that make exactly that skip. An edge can be
// several transcripts' shared (e.g. constitutive) junction, so the set can have more than one
// member; a gene with no body layer at all contributes no edges (there is no reference backbone to
// test a "skip" against).
using SpliceEdgeMap = std::map<std::pair<int64_t, int64_t>, std::set<uint32_t>>;

// D063 body-path geometry. Node ids are opaque identifiers, not genomic coordinates. A d46 body
// can also be split into several raw fragments at haplotype breaks. Under D063's fragment-union
// rule, ownership needs only (a) the union of nodes across the canonical transcript's body paths
// and (b) the union of ordinary adjacent body edges.
struct NodePairHash {
    size_t operator()(const std::pair<int64_t, int64_t>& value) const {
        const size_t first = std::hash<int64_t>{}(value.first);
        const size_t second = std::hash<int64_t>{}(value.second);
        return first ^ (second + static_cast<size_t>(0x9e3779b9U) + (first << 6U) + (first >> 2U));
    }
};

struct BodyGeometryIndex {
    std::unordered_set<int64_t> nodes;
    std::unordered_set<std::pair<int64_t, int64_t>, NodePairHash> adjacent_edges;
};

inline std::pair<int64_t, int64_t> undirected_node_pair(int64_t a, int64_t b) {
    return a <= b ? std::make_pair(a, b) : std::make_pair(b, a);
}

inline void add_body_path(BodyGeometryIndex& index, const std::vector<int64_t>& node_ids) {
    for (size_t position = 0; position < node_ids.size(); ++position) {
        index.nodes.insert(node_ids[position]);
        if (position > 0) {
            index.adjacent_edges.insert(
                undirected_node_pair(node_ids[position - 1], node_ids[position]));
        }
    }
}

inline BodyGeometryIndex index_body_paths(const std::vector<std::vector<int64_t>>& body_paths) {
    BodyGeometryIndex index;
    for (const auto& body_path : body_paths) {
        add_body_path(index, body_path);
    }
    return index;
}

// An exon edge (a,b) is a splice owned by this canonical transcript iff:
//   1. no raw body path contains an adjacent occurrence of a and b (ordinary body adjacency is a
//      conservative veto, including when repeated nodes also create a separated occurrence); and
//   2. both endpoint nodes occur in the union of body fragments. With no adjacent occurrence,
//      this covers both internal steps on one path and endpoints split across paths.
// Pair normalization makes the rule path-orientation/strand independent. The adjacency veto makes
// repeated-node and mixed-copy geometry conservative instead of declaring an ambiguous edge a
// splice merely because one occurrence pair happens to be separated.
inline bool body_geometry_owns_splice_edge(const BodyGeometryIndex& body, int64_t a, int64_t b) {
    if (a == b) {
        return false;
    }
    const auto edge = undirected_node_pair(a, b);
    return body.adjacent_edges.count(edge) == 0 && body.nodes.count(a) != 0 &&
           body.nodes.count(b) != 0;
}

// D061: the read's splice-concordant transcript set, from the undirected node-id pairs its own
// alignment crosses (main.cpp's collect_read_node_pairs: consecutive aligned nodes, within one
// subpath's mapping list or across a subpath next/connection link -- checked here as a plain
// membership test against splice_edges, exactly as the caller found them; a pair not in the map is
// an ordinary intra-exon step and imposes no constraint). Concordance is the intersection, over
// every crossed splice edge, of that edge's owner set: a transcript is concordant only if it owns
// EVERY splice the read makes, mirroring STAR's classifyAlign ("even for exon/intron aligns, sjs
// have to be concordant, otherwise align is not consistent with this transcript model").
//
// Returns nullopt (unconstrained -- every transcript is concordant) iff the read crosses no splice
// edge at all, so a single-exon or exon-internal read classifies exactly as before this feature.
// Otherwise returns the (possibly empty) intersection; empty is a real, expected outcome -- e.g. the
// read's splices belong to two different transcripts/genes and no single one owns them all -- not a
// signal to fall back to "unconstrained".
inline std::optional<std::set<uint32_t>> splice_concordant_transcripts(
    const std::vector<std::pair<int64_t, int64_t>>& read_node_pairs, const SpliceEdgeMap& splice_edges) {
    std::optional<std::set<uint32_t>> concordant;
    for (const auto& [a, b] : read_node_pairs) {
        const auto key = a <= b ? std::make_pair(a, b) : std::make_pair(b, a);
        const auto edge = splice_edges.find(key);
        if (edge == splice_edges.end()) {
            continue;  // not a splice edge -- an ordinary intra-exon step, no constraint from it
        }
        if (!concordant.has_value()) {
            concordant = edge->second;  // first crossed splice edge: seed with its owners
            continue;
        }
        if (concordant->empty()) {
            continue;  // already empty -- intersecting further can only stay empty
        }
        std::set<uint32_t> narrowed;
        std::set_intersection(concordant->begin(), concordant->end(), edge->second.begin(),
                              edge->second.end(), std::inserter(narrowed, narrowed.begin()));
        concordant = std::move(narrowed);
    }
    return concordant;
}

// Classify a read group's compatible transcripts by intron touch (ledger_transcript_rebuild_spec.md
// steps 3-6): main.cpp has already collapsed the read's per-reference-path score tally to--
//   exon_score:  transcript target id -> its best (max-collapsed across haplotype copies) exon-path
//                score.
//   body_score:  gene index -> its best (max-collapsed across haplotype copies) gene-body-path
//                score.
//   body_range:  gene index -> (lo, hi), the read's touched gene-body node-id range. Present only
//                for genes the read's alignments actually touched on that gene's body.
//   transcript_spans: gene index -> that gene's exon transcripts' own on-body exon [lo, hi] spans
//                (the load-time precompute). A gene absent here (no annotated exon transcript at
//                all) contributes no calls even if body-compatible -- there is no transcript to
//                attribute the compatibility to.
//   flank: kIntronFlankBases, the alignment-slop guard.
//   splice_concordant: (D061) the read's splice-concordant transcript set from
//                splice_concordant_transcripts, or nullopt if the read crossed no splice edge.
//                Defaults to nullopt (unconstrained) so existing callers are unaffected.
//
// top = max over every exon_score and body_score value. A transcript T is spliced iff
// exon_score[T] >= top - flank AND T is splice-concordant (owns every splice edge the read crosses;
// vacuously true when splice_concordant is nullopt). A transcript that ties the exon score but fails
// concordance is not spliced -- and because a spliced read cannot also be an unspliced (pre-mRNA)
// read, the U loop below is likewise gated on concordance, so a concordance-failed transcript is
// absent (incompatible), NOT made unspliced (which would spuriously make its gene ambiguous and drop a
// legitimately spliced read from Gene). Separately, for every gene G with body_score[G] >= top - flank
// (the read's best gene(s)), every one of G's splice-concordant transcripts that spans the read's
// touched body range and is not already spliced is unspliced. The result is
// the union, deduped by transcript_target_id (a transcript spliced via its own exon path is never
// also downgraded to unspliced).
inline std::vector<LedgerCall> classify_ledger_group(
    const std::map<uint32_t, int64_t>& exon_score, const std::map<uint32_t, int64_t>& body_score,
    const std::map<uint32_t, std::pair<int64_t, int64_t>>& body_range,
    const std::unordered_map<uint32_t, std::vector<TranscriptSpan>>& transcript_spans, int64_t flank,
    const std::optional<std::set<uint32_t>>& splice_concordant = std::nullopt) {
    int64_t top = std::numeric_limits<int64_t>::min();
    for (const auto& [transcript, score] : exon_score) {
        top = std::max(top, score);
    }
    for (const auto& [gene, score] : body_score) {
        top = std::max(top, score);
    }

    std::map<uint32_t, bool> emitted;  // transcript target id -> spliced, sorted + deduped
    for (const auto& [transcript, score] : exon_score) {
        if (score < top - flank) {
            continue;
        }
        if (splice_concordant.has_value() && splice_concordant->count(transcript) == 0) {
            continue;  // D061: ties the exon score, but does not own a splice edge the read crosses
        }
        emitted[transcript] = true;
    }
    for (const auto& [gene, score] : body_score) {
        if (score < top - flank) {
            continue;
        }
        const auto rng = body_range.find(gene);
        if (rng == body_range.end()) {
            continue;
        }
        const auto spans = transcript_spans.find(gene);
        if (spans == transcript_spans.end()) {
            continue;  // no annotated transcript for this gene -- nothing to attribute it to
        }
        for (const TranscriptSpan& span : spans->second) {
            if (span.lo <= rng->second.first && span.hi >= rng->second.second) {
                if (splice_concordant.has_value() &&
                    splice_concordant->count(span.transcript_target_id) == 0) {
                    continue;  // D061: the read makes a splice this transcript does not own, so the read
                               // is INCOMPATIBLE with it (a spliced read is not an unspliced/pre-mRNA
                               // read) -- neither S nor U, absent. Without this gate a concordance-failed
                               // transcript would fall through to U here and spuriously make its gene
                               // ambiguous, dropping a legitimately spliced read from Gene.
                }
                emitted.emplace(span.transcript_target_id, false);  // no-op if already spliced
            }
        }
    }

    std::vector<LedgerCall> calls;
    calls.reserve(emitted.size());
    for (const auto& [transcript, spliced] : emitted) {
        calls.push_back({transcript, spliced});
    }
    return calls;
}

}  // namespace pathtally
