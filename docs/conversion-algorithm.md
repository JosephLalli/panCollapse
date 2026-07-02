# GAMP-to-RAD Conversion Algorithm

panCollapse converts a name-grouped GAMP into a RAD file for alevin-fry. For each read it
reports the set of transcripts the read is compatible with, each with an orientation, and
writes that to RAD. This is graph-native transcript feature counting on a pangenome: the
question is "which transcripts is this read compatible with," answered directly from the
graph the mapper already produced.

This document is the canonical algorithm, governed by decision D048.

## Inputs (runtime)

- Name-grouped GAMP: all records for one read name are contiguous.
- The graph (`.xg` or equivalent) carrying the haplotype-specific transcript (HST) paths
  that `vg rna` embedded (`<transcript_id>_H<n>` / `_R<n>`).
- A transcript-to-gene map (t2g) for `tx2gene.tsv`.

A GTF and `vg rna` build the annotated graph during reference/fixture creation only; they
are not runtime inputs, and there is no separate collapse manifest.

## Per-read algorithm

For each read, across all of its alignments in the GAMP (the primary record and every
supplementary/secondary record):

1. **Score each node.** For every node the read aligns to, compute that node's alignment
   score under vg's own scheme, recomputed from the `Mapping` edits: match `+1`/base,
   mismatch `-4`/base, affine gaps (open `6`, extend `1`), and the `+5` full-length bonus at
   read ends. Carry affine-gap state and the read-end bonus across node boundaries. vg does
   not serialize per-node scores, but the per-node contributions of a subpath sum to that
   subpath's `Subpath.score` — the built-in correctness check.

2. **Attribute to transcripts.** Every HST path that traverses a scored node receives that
   node's score (native `for_each_step_on_handle`). Compatibility is graph membership: the
   HST path visits the node. The HST path already encodes the transcript's spliced
   structure, so there is no exon/intron/junction test.

3. **Pick winners across all alignments.** Pool the per-HST tallies over all of the read's
   alignments. Take the single highest HST score. The winners are every HST tied at that
   score, drawn from any alignment.

4. **Collapse to transcript IDs.** Reduce the winning HSTs to their unique transcript IDs
   (drop the `_H<n>` / `_R<n>` haplotype suffix). This unique set is the read's compatible
   targets — RAD `refs`.

5. **Record orientation.** For each winning HST, the read is forward if it traverses the HST
   path's nodes in the path's direction, reverse if opposite. HST paths are stored 5'->3' of
   the transcript, so this is the read's sense/antisense relationship directly. Per
   transcript, if the winning HSTs disagree, record the orientation backed by the majority of
   aligned bases (deterministic forward fallback for an exact tie). Encode into RAD `dirs`
   (forward = high bit of `compressed_ori_refid`).

6. **Emit.** Write the RAD record: raw cell barcode and UMI parsed from the read name,
   `refs`, and `dirs`.

A read whose records carry no aligned subpaths, or whose alignments cross no HST node, emits
no RAD record and increments the corresponding summary counter.

## Scoring: replicating vg exactly

vg computes a subpath score as a left-to-right sum over the subpath's mappings (nodes) and
edits (`vg` source `alignment_scorer.cpp:score_partial_alignment`; defaults in
`alignment_scorer.hpp`). Because it is additive over nodes, we reproduce it per node and
attribute each node's contribution to the HSTs crossing it. Summed over a subpath, our
per-node scores must equal `Subpath.score`; that equality is the acceptance test for the
scorer. If vg mapped with base-quality-adjusted scoring, the flat per-node sum will not match
`Subpath.score` and the quality-adjusted matrix must be reproduced instead; the checksum
reveals which regime applies.

The default per-node scorer is the flat scheme. An optional `--score qualadj` selects a
reimplementation of vg's QualAdjAligner (`src/pathtally_qualadj.hpp`) that reproduces
`Subpath.score` for 99.2% of subpaths on the MHC GAMP (residual +/-1 is score-matrix
rounding). The flat scorer is sufficient for ranking; qualadj is available when exact
fidelity to vg's quality-adjusted scores is wanted.

## Output and determinism

- `map.rad` via the streaming writer, `tx2gene.tsv` from the t2g, and a run summary with
  stable counters.
- Single-threaded; the RAD target dictionary is lexicographically ordered; output is
  byte-reproducible across runs.
- alevin-fry consumes `map.rad` downstream (permit-list, collate, quant).

## Implementation increments

Build one testable behavior at a time:

1. Reproduce vg's per-node alignment score from the `Mapping` edits; accept when per-node
   sums equal `Subpath.score` on real GAMP subpaths.
2. Attribute each node's score to the HST paths crossing it; collapse HST names to unique
   transcript IDs.
3. Select winners: the single top HST score across all of a read's alignments, plus ties.
4. Record per-transcript orientation by the majority of aligned bases into RAD `dirs`.
5. Emit `map.rad` (streaming writer) and `tx2gene.tsv`; verify alevin-fry consumption.
6. Human-pangenome MHC fixture (`docs/testing_fixture_creation.md`): slice HPRC v1.1 GRCh38,
   build the spliced/HST graph with `vg rna`, simulate reads, map to GAMP, and compare
   panCollapse RAD against the independent GAMP-driven oracle.

Per-increment verification is in `docs/conversion-algorithm-tests.md`.

## Open items

- Step 1 is verified on real MHC GAMP: the flat per-node scorer reproduces `Subpath.score`
  exactly for 93.1% of subpaths; the remainder are lower-quality bases whose mismatch penalty
  vg attenuated under base-quality-adjusted mapping (deviating subpaths mean base quality 22.4
  vs 34.4 for exact matches). The flat score is adopted as the per-node weight; exact
  quality-adjusted reproduction is deferred as unnecessary for ranking.
