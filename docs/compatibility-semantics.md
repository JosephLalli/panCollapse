# Transcript Compatibility Semantics

This file is the behavioral reference for transcript compatibility. The full GAMP-to-RAD
algorithm is in `docs/conversion-algorithm.md`; both are governed by decision D048.

## Core rule

A read is compatible with a transcript when a haplotype-specific transcript (HST) path for
that transcript, embedded by `vg rna`, traverses a node the read aligns to. Compatibility is
graph-path membership: the HST path already encodes the transcript's spliced structure, so
there is no separate exon, intron, or splice-junction test at runtime and no annotation
projection.

Each node the read aligns to is scored under vg's own alignment scheme (see
`docs/conversion-algorithm.md`), and that score is added to every HST path crossing the node.
A transcript's evidence is the score of its best explicit path/Parent identity.

## Winner selection and collapse

- Score every HST across all of the read's alignments (primary and supplementary/secondary).
- Production resolves exact `vg_path_name -> unique_parent -> canonical_transcript -> gene_id`.
  MAX-collapse paths within each Parent, then Parents within each canonical transcript; retain
  every tied winning path and Parent. Canonical transcripts tied at the global top score are the
  read's targets.
- Treat path and identity strings as opaque. In particular, a literal transcript ending `_R1`
  is unchanged.
- Never sum alternative path or Parent evidence. Copy number and fragmented emitted paths cannot
  manufacture a higher transcript score.
- `--legacy-adapter hst-v1` alone preserves D062/D063's two-/three-column t2g and suffix behavior;
  there is no automatic legacy detection.

## Orientation

A read is forward on a transcript when it traverses the HST path's nodes in the path's
direction, reverse when opposite. HST paths are stored 5'->3', so this is sense/antisense
directly. If a transcript's winning HSTs disagree, record the orientation backed by the
majority of aligned bases (deterministic forward fallback for an exact tie). RAD `dirs`
carries this; downstream alevin-fry applies expected-orientation filtering.

## No state classification

Default `score` mode does not label molecules spliced or unspliced. Ledger count modes classify
each transcript as `S` or `U` from its explicit exon/body layers and emit that state in BAM `GL`;
gene pooling remains downstream. panCollapse writes the resulting ordinary two-column canonical
`tx2gene.tsv` in either mode.

Exact `genefull_ex50pas` is the D068 scoring exception to the ordinary HST tally above. For each
Parent-specific exon/body model it scores complete compatible traversals directly from GAMP
subpath and connection scores, keeps candidates within five points of the read group's global
compatible optimum, and only then emits E/P/B plus orientation for downstream six-rank selection.
This is the v0.8 default. `--no-ex50-score-window` skips the pruning step and sends every complete
compatible exact traversal to E/P/B-orientation ordering, restoring pre-D068 eligibility for a
sensitivity comparison. This filter affects exact BAM evidence only, not the established RAD
compatibility path.
