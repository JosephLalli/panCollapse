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
- Streaming RAD-to-disk writing (D049): the header, target dictionary, and tags are written
  up front with a placeholder `num_chunks`; records stream to `map.rad` as groups flush; and
  the writer seeks back at finalize to patch the exact chunk byte/record counts and
  `num_chunks`. A run with no emitted record leaves a header-only file (`num_chunks = 0`).
- Byte-identical output for identical inputs/configuration. The converter is single-threaded
  under D045. This guarantee applies fully to the default flat scorer, which is integer
  arithmetic throughout and produces bit-for-bit identical RAD on any conforming platform.
  Under `--score qualadj`, the score matrix and full-length-bonus table are constructed with
  `std::exp`, `std::log`, `std::pow`, and `std::round` (`src/pathtally_qualadj.hpp`
  `build_matrix`/`build_bonuses`); the last-bit rounding of those libm functions can differ
  across platforms or libm versions. A difference of one in a matrix entry can flip a score
  tie and change RAD bytes across machines. Within one machine and build, qualadj output
  remains deterministic.
- GAMP read groups whose records all have empty subpaths are unaligned: no RAD record, and
  `unaligned_reads` is incremented.

Conceptually each record contains `bc`, `umi`, `refs`, and `dirs`; `refs` is the target
compatibility set (not coordinates) and `dirs` is parallel to `refs`, consumed by
alevin-fry expected-orientation filtering.

### Companion artifacts

- `tx2gene.tsv` from the t2g;
- configured raw barcode and UMI lengths, which set RAD `cblen` and `ulen`;
- a run summary with the product-spec Section 12 counters, including groups skipped and
  unaligned read groups; the field `emitted_target_count_histogram` holds a
  semicolon-separated list of `target_count:group_count` pairs (ascending by target count)
  recording how many emitted groups had each distinct number of compatible targets;
- version/build information and input identities/checksums where practical.

### Optional BAM (`--bam-out`)

An opt-in BAM for a CellRanger-style counting stack (`umi_tools count --per-gene --gene-tag=XT`,
then DropletUtils `emptyDropsCellRanger`). It does not change the RAD: `map.rad` is byte-identical
with or without `--bam-out`. One mapped record per emitted read carries 10x tags
(`CB`/`CR`, `UB`/`UR`, full-set `GX`, `GN`, and single-gene-or-omitted `XT`) at a nominal position
on a synthetic per-gene contig; positions are ignored by the counter and genes come from the t2g,
not a linear reference. Full contract in [`bam-export.md`](bam-export.md).

## Exit behavior

Hard failure is expected for:

- unreadable or incompatible inputs;
- GAMP name-grouping violations (a completed name recurs);
- a GAMP/xg node-id-space mismatch: if any aligned node ID is absent from the graph the run aborts
  with a non-zero exit; a node that exists but lies on no HST path is legitimate and produces no
  target hits for that node (not an error);
- a graph missing the HST paths, or a t2g inconsistent with the emitted transcripts;
- inability to encode standards-conformant RAD.

Skippable per-read conditions are missing, malformed, or unsupported raw CB/UMI values; they
are counted (`raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
`raw_molecule_unsupported_groups`, `raw_molecule_skipped_groups`) and become fatal under
`--molecule-identity-failures fail`. Unaligned read groups (all-empty subpaths) are not
molecule-identity failures: no record, counted under `no_compatible_transcript_groups` and
`unaligned_reads`.

GAMP input streams from a file or from stdin via `--gamp -` (D049), so `vg mpmap ... |
panCollapse convert --gamp - ...` works with the same grouping/recurrence validation. A
stdout RAD mode is not in the active CLI, because the seek-and-backpatch writer needs a
seekable output file.
