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
  `<original_read_name>_<raw_CB>_<raw_UMI>_cy<hex(CY)>_uy<hex(UY)>`. Raw barcode, UMI, and optional
  barcode quality are parsed from the right; panCollapse does not correct them. Legacy
  names without the quality suffix remain accepted.
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

### Production path identity ledger

`--path-identity-ledger` accepts only the headered schema `panSC-path-identity-v1`, one row per
emitted vg path. Required columns are:

`schema_version`, `vg_path_name`, `vg_path_length`, `vg_haplotype_origins`, `unique_parent`,
`source_parent`, `input_parent`, `canonical_transcript`, `gene_id`, `source_path_or_contig`,
`sample`, `haplotype`, `annotation_source`, `feature_layer`, `selection_status`,
`fallback_status`, `exon_unique_parent`, `gene_locus`, `source_gene`, `source_transcript`,
`transcript_class`, `strand`, `start`, and `end`.

That header and order are exact; missing, extra, duplicated, or reordered columns are rejected so
the consumer cannot drift from the panSC writer contract.

All required fields are nonempty; `feature_layer` is `exon` or `body`, strand is `+` or `-`,
coordinates and path length are positive integers with `start <= end`, and canonical transcript
and gene are resolved identities rather than `N/A`. `unique_parent` is globally layer-disjoint
and has one annotation identity, while allowing several emitted paths with that identical Parent
identity. A canonical transcript maps to one gene. Exon rows self-reference
`exon_unique_parent`; each body row crosslinks to an existing exon Parent with the same canonical
transcript and gene. For `source_transcript=N/A`, fallback is explicit and the canonical identity
is the exon Parent (`unique_parent` on exon rows, `exon_unique_parent` on body rows).

Every `vg_path_name` must exist exactly in the XG and have the recorded `vg_path_length`; extra
non-ledger genomic XG paths are allowed. Names are opaque, so `_H<n>`/`_R<n>` are never stripped.
Tabs/newlines are illegal in all fields. Because BAM provenance uses semicolon-separated groups
and comma-separated ties, those delimiters are reserved in `vg_path_name` and `unique_parent`;
semicolon is also reserved in `canonical_transcript` and `gene_id`. Commas remain legal in
general provenance, including `vg_haplotype_origins`.

### Explicit legacy adapter

Historical `--t2g` and `--body-t2g` inputs require `--legacy-adapter hst-v1`; no file-shape
auto-detection exists. The exon t2g is
`graph_path<TAB>gene_id[<TAB>canonical_transcript_id]`. Its two-column form strips a terminal
`_H<n>`/`_R<n>`; column 3 is an exact raw-path alias. Conflicting path/transcript or
transcript/gene mappings remain hard errors.

A GTF and `vg rna` build the annotated graph during reference/fixture creation only; they
are not runtime inputs.

### Count mode and body identity

The default `--count-mode score` uses exon rows. The ledger count modes (`gene`, `genefull`,
`genefull_exonoverintron`, `genefull_ex50pas`) require body rows in the production path ledger.
When a production ledger count mode also requests `--bam-out`, the versioned typed union requires
at least one body row linked to every exon Parent. This completeness check is mode-specific:
score mode and non-BAM RAD conversion do not promise exact Ex50 evidence and therefore do not
impose it.
Under `hst-v1`, the first three additionally take `--body-t2g`:

- two columns, `path<TAB>gene`, preserve the legacy gene-body/span classifier;
- three columns, `raw_body_path<TAB>gene<TAB>canonical_transcript`, enable D063's
  transcript-first classifier. Every row in a body t2g must use the same width. Its canonical
  transcript must occur in the exon t2g with the same gene. Multiple raw body paths, including
  haplotype copies or fragment-local segments, may map to one transcript and MAX-collapse.

Exact `genefull_ex50pas` is production-only and requires `--bam-out --bam-multigene all`;
`hst-v1` is a hard error. It emits exact base-overlap evidence rather than the S/U ledger:
parallel `TX`/`GX`/`GD`/`GT`/`XP`/`XU`, with one exact exon path/Parent for E/P or linked body
path/Parent for B per slot. Exactly 50% exonic is B, and a fully exonic but splice-discordant
alignment is P. Repeated canonical `TX` values are valid because locus Parents and alignment
alternatives are not collapsed before count_cr's six-rank selection.
By default, only complete compatible alternatives within five score points of the global
compatible optimum reach that selection. `--no-ex50-score-window` restores all complete compatible
alternatives for a pre-D068 sensitivity comparison. The BAM header and `summary.tsv` record which
policy was used.

Production and legacy annotation name paths already embedded in the graph; there is no node->gene
map. A transcript-first count uses `--bam-out --bam-multigene all` so the complete transcript
ledger reaches count_cr, which performs transcript-to-gene pooling. See
[`genefull.md`](genefull.md).

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
- In production, exact paths MAX-collapse within `unique_parent`, Parents MAX-collapse within
  `canonical_transcript`, and canonical transcripts tied at the global top score become RAD
  targets. Scores are never summed; all tied winning paths and Parents remain BAM provenance.
  `hst-v1` preserves D062/D063's tested selection behavior.
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
  `unaligned_reads` is incremented. With `--bam-out`, every valid molecule group still emits
  exactly one BAM record for downstream barcode correction; featureless groups use the
  unmapped `XB:Z:barcode_only` layout below.

Conceptually each record contains `bc`, `umi`, `refs`, and `dirs`; `refs` is the target
compatibility set (not coordinates) and `dirs` is parallel to `refs`, consumed by
alevin-fry expected-orientation filtering.

### Companion artifacts

- `tx2gene.tsv` from the ledger projection (or legacy t2g);
- configured raw barcode and UMI lengths, which set RAD `cblen` and `ulen`;
- a run summary with the product-spec Section 12 counters, including groups skipped and
  unaligned read groups, `strand_filtered_groups` (reads dropped by `--strand`) and
  `multigene_dropped_groups` (reads dropped by the ledger `Unique` rule), `bam_records`, and
  `barcode_only_bam_records`; the field
  `emitted_target_count_histogram` holds a semicolon-separated list of
  `target_count:group_count` pairs (ascending by target count) recording how many emitted
  groups had each distinct number of compatible targets;
- version/build information and input identities/checksums where practical.

### Optional BAM (`--bam-out`)

An opt-in BAM for a CellRanger-style counting stack. It does not change the RAD: `map.rad` is
byte-identical with or without `--bam-out`. Every valid input read group has exactly one BAM
record. Feature-bearing records are mapped nominally to a synthetic gene contig and carry
`CB`/`CR`, `UB`/`UR`, optional raw qualities `CY`/`UY`, plus the feature ledger. A group with
no BAM count feature instead emits one unmapped `XB:Z:barcode_only` record with only molecule
tags. That record contributes to STARsolo's pre-feature barcode-abundance prior and must never
be treated as gene evidence. Production-ledger feature records additionally carry `TX` and its
parallel `XP`/`XU` provenance groups. Ordinary count modes carry `GL`; exact
`genefull_ex50pas` carries `GT` instead. Full contract in
[`bam-export.md`](bam-export.md).

## Exit behavior

Hard failure is expected for:

- unreadable or incompatible inputs;
- GAMP name-grouping violations (a completed name recurs);
- a GAMP/xg node-id-space mismatch: if any aligned node ID is absent from the graph the run aborts
  with a non-zero exit; a node that exists but lies on no HST path is legitimate and produces no
  target hits for that node (not an error);
- a production ledger with missing/invalid fields, identity conflicts, layer/crosslink errors,
  reserved identifier delimiters, an XG-missing exact path, or an XG path-length mismatch;
- a graph missing the HST paths, or an explicit `hst-v1` t2g inconsistent with the emitted
  transcripts;
- inability to encode standards-conformant RAD.

Skippable per-read conditions are missing, malformed, or unsupported raw CB/UMI values, or a
malformed encoded barcode-quality suffix; they
are counted (`raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
`raw_molecule_unsupported_groups`, `raw_molecule_skipped_groups`) and become fatal under
`--molecule-identity-failures fail`. Unaligned read groups (all-empty subpaths) are not
molecule-identity failures: no record, counted under `no_compatible_transcript_groups` and
`unaligned_reads`.

GAMP input streams from a file or from stdin via `--gamp -` (D049), so `vg mpmap ... |
panCollapse convert --gamp - ...` works with the same grouping/recurrence validation. A
stdout RAD mode is not in the active CLI, because the seek-and-backpatch writer needs a
seekable output file.
