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
A transcript's evidence is the score of its best HST.

## Winner selection and collapse

- Score every HST across all of the read's alignments (primary and supplementary/secondary).
- The read's targets are the HSTs tied at the single highest score, pooled across all of the
  read's alignments.
- Collapse those winning HSTs to their unique transcript IDs; haplotype copies of one
  transcript collapse to one ID. This unique set is the read's compatible targets.
- Do not multiply evidence because one transcript is represented by several HST paths.

## Orientation

A read is forward on a transcript when it traverses the HST path's nodes in the path's
direction, reverse when opposite. HST paths are stored 5'->3', so this is sense/antisense
directly. If a transcript's winning HSTs disagree, record the orientation backed by the
majority of aligned bases (deterministic forward fallback for an exact tie). RAD `dirs`
carries this; downstream alevin-fry applies expected-orientation filtering.

## No state classification

panCollapse does not label molecules spliced or unspliced and does not filter to
transcript- or gene-unique modes. It emits the full compatible transcript set per read. The
two-column transcript-to-gene map projects targets to genes downstream.
