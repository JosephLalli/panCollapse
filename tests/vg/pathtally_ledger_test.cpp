// Unit tests for classify_ledger_group (D060): per-transcript spliced/unspliced classification by
// intron touch. Pure: builds small exon_score/body_score/body_range/transcript_spans maps directly
// (no graph, no GAMP) and checks the returned per-transcript calls. Also covers D061 (splice-junction
// concordance): splice_concordant_transcripts (the read-side intersection helper) and the new gate
// it feeds into classify_ledger_group -- still pure, no graph, no GAMP.

#include "pathtally_ledger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using pathtally::classify_ledger_group;
using pathtally::LedgerCall;
using pathtally::splice_concordant_transcripts;
using pathtally::SpliceEdgeMap;
using pathtally::TranscriptSpan;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

// Sort by transcript id for order-independent comparison (classify_ledger_group already returns
// sorted, ascending by construction, but assert on the fact directly rather than assuming it).
bool calls_equal(std::vector<LedgerCall> calls, std::vector<LedgerCall> expected) {
    auto by_id = [](const LedgerCall& a, const LedgerCall& b) {
        return a.transcript_target_id < b.transcript_target_id;
    };
    std::sort(calls.begin(), calls.end(), by_id);
    std::sort(expected.begin(), expected.end(), by_id);
    if (calls.size() != expected.size()) return false;
    for (size_t i = 0; i < calls.size(); ++i) {
        if (calls[i].transcript_target_id != expected[i].transcript_target_id ||
            calls[i].spliced != expected[i].spliced) {
            return false;
        }
    }
    return true;
}

constexpr int64_t kFlank = 5;

void run() {
    // Single isoform, purely exonic (genefull_smoke "exonread"): transcript 1 (gene 10) ties top via
    // its own exon path; the gene body also ties top (exon nodes are on-body) and transcript 1's span
    // brackets the touched range, but it is already spliced so the body pass adds nothing new.
    {
        std::map<uint32_t, int64_t> exon_score{{1, 20}};
        std::map<uint32_t, int64_t> body_score{{10, 20}};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{{10, {1, 3}}};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{{10, {{1, 3, 1}}}};
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank);
        check(calls_equal(calls, {{1, true}}), "purely exonic: transcript 1 spliced-only");
    }

    // Single isoform, purely intronic (genefull_smoke "intronread"): no exon path is touched at all
    // (exon_score empty), only the gene body ties top; transcript 1's span brackets the touched
    // range and is not already spliced -> unspliced.
    {
        std::map<uint32_t, int64_t> exon_score{};
        std::map<uint32_t, int64_t> body_score{{10, 10}};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{{10, {2, 2}}};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{{10, {{1, 3, 1}}}};
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank);
        check(calls_equal(calls, {{1, false}}), "purely intronic: transcript 1 unspliced-only");
    }

    // Cross-isoform ambiguous: gene 10 has two transcripts (1, 2) both spanning [1,3]. The read ties
    // top via transcript 2's own exon path (a retained-intron isoform); transcript 1 does not touch
    // any of its own exon nodes but its span still brackets the read, so it is unspliced -- same
    // gene ends up with a spliced AND an unspliced transcript (count_cr derives ambiguity from this).
    {
        std::map<uint32_t, int64_t> exon_score{{2, 10}};
        std::map<uint32_t, int64_t> body_score{{10, 10}};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{{10, {2, 2}}};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{{10, {{1, 3, 1}, {1, 3, 2}}}};
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank);
        check(calls_equal(calls, {{1, false}, {2, true}}),
              "cross-isoform: transcript 1 unspliced, transcript 2 spliced, same gene");
    }

    // Multi-gene: the read ties top for gene 20 via transcript 3's own exon path (its only exon IS
    // its whole body) and for gene 10's body only (no exon touch) -- transcript 1 (gene 10) becomes
    // unspliced, transcript 3 (gene 20) stays spliced; two different genes.
    {
        std::map<uint32_t, int64_t> exon_score{{3, 10}};
        std::map<uint32_t, int64_t> body_score{{10, 10}, {20, 10}};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{{10, {2, 2}}, {20, {2, 2}}};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{{10, {{1, 3, 1}}},
                                                                        {20, {{2, 2, 3}}}};
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank);
        check(calls_equal(calls, {{1, false}, {3, true}}), "multi-gene: transcript 1 U, transcript 3 S");
    }

    // A gene compatible only through its body but with NO annotated exon transcript (absent from
    // transcript_spans) contributes no calls -- there is no transcript id to attribute it to, so it
    // is silently excluded rather than fabricated.
    {
        std::map<uint32_t, int64_t> exon_score{};
        std::map<uint32_t, int64_t> body_score{{30, 10}};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{{30, {2, 2}}};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{};  // gene 30 has no transcripts
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank);
        check(calls.empty(), "transcript-less gene: no calls emitted");
    }

    // Flank tolerance: a transcript scoring within kIntronFlankBases of top is still spliced (the
    // velocyto MIN_FLANK / STARsolo minOverlapMinusOne alignment-slop guard).
    {
        std::map<uint32_t, int64_t> exon_score{{1, 16}};  // top(20) - 4 <= flank(5)
        std::map<uint32_t, int64_t> body_score{{10, 20}};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{{10, {1, 3}}};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{{10, {{1, 3, 1}}}};
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank);
        check(calls_equal(calls, {{1, true}}), "flank tolerance: near-top exon score still spliced");
    }

    // Empty input (no reference touched at all) -> no calls.
    {
        std::map<uint32_t, int64_t> exon_score{};
        std::map<uint32_t, int64_t> body_score{};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{};
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank);
        check(calls.empty(), "empty tally: no calls emitted");
    }

    // D061 (splice-junction concordance), gated on classify_ledger_group's new optional parameter --
    // omitting it (every case above) must still classify exactly as before (the default is nullopt =
    // unconstrained), which the untouched cases above already prove by construction.

    // Two transcripts of the same gene tie the exon score exactly as the "purely exonic" case above,
    // but only transcript 1 is splice-concordant: transcript 2 fails the gate. A read that makes a
    // splice transcript 2 does not own is INCOMPATIBLE with it (a spliced read is not an unspliced
    // read), so the concordance gate keeps transcript 2 out of U as well as S -- it is absent, NOT
    // demoted to unspliced. Mirrors genefull_isoform_smoke's NM_1.1 (owns the read's splice edge, S)
    // vs NM_1.2 (does not -> absent, so GENE1 is not spuriously made ambiguous).
    {
        std::map<uint32_t, int64_t> exon_score{{1, 20}, {2, 20}};
        std::map<uint32_t, int64_t> body_score{{10, 20}};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{{10, {1, 3}}};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{{10, {{1, 3, 1}, {1, 3, 2}}}};
        std::optional<std::set<uint32_t>> concordant{std::set<uint32_t>{1}};
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank, concordant);
        check(calls_equal(calls, {{1, true}}),
              "splice concordance: non-concordant transcript ties exon score but is absent (not U)");
    }

    // Same score tie, but transcript 2's gene has no body coverage at all (no span to fall back on):
    // failing concordance now means transcript 2 is dropped entirely, not merely demoted -- the "not
    // silently promoted to U" case, matching the real TUBB1 finding (no Gene credit at all). Exercised
    // end-to-end by the splice_concordance_smoke ctest fixture (GENE3/NM_3.1 has no body layer).
    {
        std::map<uint32_t, int64_t> exon_score{{1, 20}, {2, 20}};
        std::map<uint32_t, int64_t> body_score{};  // transcript 2's gene has no body coverage
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{};
        std::optional<std::set<uint32_t>> concordant{std::set<uint32_t>{1}};
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank, concordant);
        check(calls_equal(calls, {{1, true}}),
              "splice concordance: non-concordant transcript with no span to fall back on is dropped entirely");
    }

    // An empty-but-present concordant set (the read's crossed splices belong to no single transcript
    // at all) fails EVERY transcript's exon-score win AND, via the concordance gate on the body/span
    // loop, its U fallback too -- nothing is emitted (the read is incompatible with every transcript).
    {
        std::map<uint32_t, int64_t> exon_score{{1, 20}};
        std::map<uint32_t, int64_t> body_score{{10, 20}};
        std::map<uint32_t, std::pair<int64_t, int64_t>> body_range{{10, {1, 3}}};
        std::unordered_map<uint32_t, std::vector<TranscriptSpan>> spans{{10, {{1, 3, 1}}}};
        std::optional<std::set<uint32_t>> concordant{std::set<uint32_t>{}};  // present but empty
        auto calls = classify_ledger_group(exon_score, body_score, body_range, spans, kFlank, concordant);
        check(calls.empty(),
              "splice concordance: empty-but-present concordant set emits nothing (fails both S and U)");
    }

    std::printf("pathtally ledger: PASS\n");
}

// D061: splice_concordant_transcripts, the pure read-side helper that turns a read's crossed
// node-id pairs into a splice-concordant transcript set (or nullopt if it crossed no splice edge).
void run_splice_concordance() {
    // No crossed splice edge at all (both pairs are ordinary intra-exon steps, absent from the map)
    // -> nullopt (unconstrained): every transcript stays eligible, exactly as before this feature.
    {
        SpliceEdgeMap edges;
        edges[{1, 3}] = {5, 6};
        auto concordant = splice_concordant_transcripts({{1, 2}, {2, 3}}, edges);
        check(!concordant.has_value(), "no crossed splice edge: unconstrained (nullopt)");
    }

    // One crossed edge -> exactly its owner set. Direction-agnostic: the read's own pair can arrive
    // in either order (subpath traversal order), while the map key is always stored (min,max).
    {
        SpliceEdgeMap edges;
        edges[{1, 3}] = {5, 6};
        auto concordant = splice_concordant_transcripts({{3, 1}}, edges);
        check(concordant.has_value() && *concordant == std::set<uint32_t>{5, 6},
              "one crossed edge, reversed pair order: owner set returned exactly");
    }

    // Two crossed edges -> the intersection of their owner sets (a transcript concordant with only
    // one of the read's two splices is not fully concordant).
    {
        SpliceEdgeMap edges;
        edges[{1, 3}] = {5, 6, 7};
        edges[{4, 9}] = {6, 7, 8};
        auto concordant = splice_concordant_transcripts({{1, 3}, {4, 9}}, edges);
        check(concordant.has_value() && *concordant == std::set<uint32_t>{6, 7},
              "two crossed edges: intersection of owner sets");
    }

    // Two crossed edges with disjoint owners -> an empty but PRESENT set (no single transcript owns
    // both splices), not a fall-back to "unconstrained".
    {
        SpliceEdgeMap edges;
        edges[{1, 3}] = {5};
        edges[{4, 9}] = {6};
        auto concordant = splice_concordant_transcripts({{1, 3}, {4, 9}}, edges);
        check(concordant.has_value() && concordant->empty(),
              "disjoint owner sets: empty but present, not unconstrained");
    }

    // A pair absent from the map is an ordinary (non-splice) step and adds no constraint, even when
    // mixed with a real splice edge.
    {
        SpliceEdgeMap edges;
        edges[{1, 3}] = {5};
        auto concordant = splice_concordant_transcripts({{9, 10}, {1, 3}}, edges);
        check(concordant.has_value() && *concordant == std::set<uint32_t>{5},
              "non-splice pair mixed with a real splice edge: only the real edge constrains");
    }

    std::printf("pathtally splice concordance: PASS\n");
}

}  // namespace

int main() {
    run();
    run_splice_concordance();
    return 0;
}
