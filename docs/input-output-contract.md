# Input and Output Contract

This document translates the product specification into interface obligations without
prescribing the code architecture. The GAMP-to-RAD algorithm is defined by D048 and
`docs/conversion-algorithm.md`.

## Runtime inputs

### GAMP

- Binary VG multipath alignment stream.
- Records for a read name are contiguous; the converter validates recurrence of a
  previously completed name.
- The GAMP name field carries the observed raw molecule identity as
  `<original_read_name>_<raw_CB>_<raw_UMI>`. The raw barcode and UMI are parsed from the
  right; panCollapse does not correct either value.
- Parsed raw barcode and UMI values must match the configured lengths (`--raw-cb-length`,
  `--raw-umi-length`; Phase 2 defaults 16 and 12).
- Every alignment of the read (the primary record and each supplementary/secondary record)
  is scored. Each aligned node is scored under vg's own alignment scheme, recomputed from
  the `Mapping` edits (match/mismatch, affine gaps, read-end bonus). There is no
  complete-traversal enumeration and no traversal cap.

### Graph with HST paths

- An `.xg` (or equivalent) for the same graph/node-id space that produced the GAMP.
- The graph must expose the haplotype-specific transcript (HST) paths that `vg rna`
  embedded (`<transcript_id>_H<n>` / `_R<n>`), queried through `for_each_step_on_handle`.
- After GAMP exists, `.gcsa`, `.gcsa.lcp`, `.dist`, `.snarls`, minimizers, GBZ, and GBWT
  are not panCollapse inputs.
- V1 does not build a panCollapse-specific persistent index.

### Transcript-to-gene map (t2g)

- A two-column `transcript_id<TAB>gene_id` map, used to write `tx2gene.tsv` and to project
  RAD targets to genes downstream.
- Transcript-copy collapse is implicit in HST path naming: HST paths sharing a transcript
  ID collapse to that one transcript. There is no separate runtime collapse manifest.

A GTF and `vg rna` build the annotated graph during reference/fixture creation only; they
are not runtime inputs.

### Barcode and UMI

The raw cell barcode and raw UMI come from the GAMP name field. Values are written to RAD as
observed; panCollapse does not correct, permit-list, repair, or mix them. Missing,
malformed, or unsupported values (including length mismatch against the configured lengths)
are skipped per read group and counted by default; `--molecule-identity-failures fail` makes
them fatal.

## Outputs

### RAD

- Mapper-style, uncollated RAD with a target dictionary of the emitted transcript IDs.
- Raw cell barcode and raw UMI encoded in the schema alevin-fry expects.
- Each read's target set is the winning HSTs — the top HST score across all of the read's
  alignments plus ties — collapsed to unique transcript IDs, encoded as zero-based target
  IDs into the RAD header.
- One orientation value per emitted target: forward if the read runs along the HST path,
  reverse if opposite; on disagreement among a transcript's winning HSTs, the majority of
  aligned bases decides (deterministic forward fallback for an exact tie).
- No transcript likelihood weights and no splicing-state labels.
- Streaming RAD-to-disk writing: `num_chunks = 0`, file-tag values, then complete chunks
  emitted incrementally to `map.rad`.
- Byte-identical output for identical inputs/configuration. The converter is single-threaded
  under D045.
- GAMP read groups whose records all have empty subpaths are unaligned: no RAD record, and
  `unaligned_reads` is incremented.

Conceptually each record contains `bc`, `umi`, `refs`, and `dirs`; `refs` is the target
compatibility set (not coordinates) and `dirs` is parallel to `refs`, consumed by
alevin-fry expected-orientation filtering.

### Companion artifacts

- `tx2gene.tsv` from the t2g;
- configured raw barcode and UMI lengths, which set RAD `cblen` and `ulen`;
- a run summary with the product-spec Section 12 counters, including groups skipped and
  unaligned read groups;
- version/build information and input identities/checksums where practical.

## Exit behavior

Hard failure is expected for:

- unreadable or incompatible inputs;
- GAMP name-grouping violations (a completed name recurs);
- a graph missing the HST paths, or a t2g inconsistent with the emitted transcripts;
- inability to encode standards-conformant RAD.

Skippable per-read conditions are missing, malformed, or unsupported raw CB/UMI values; they
are counted (`raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
`raw_molecule_unsupported_groups`, `raw_molecule_skipped_groups`) and become fatal under
`--molecule-identity-failures fail`. Unaligned read groups (all-empty subpaths) are not
molecule-identity failures: no record, counted under `no_compatible_transcript_groups` and
`unaligned_reads`.

Future `vg mpmap | panCollapse` stdin support must accept the same binary GAMP semantics and
preserve grouping/recurrence validation. A stdout RAD mode is not in the active CLI.
