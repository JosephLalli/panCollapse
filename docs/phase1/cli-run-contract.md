# Phase 1 CLI And Run Contract

This document is the Phase 1 CLI snapshot required for **Gate Behavior Specified**. It
collects the user-facing run contract without forcing fixture implementers to load every
Phase 1 fixture document.

## Command Snapshot

```text
panCollapse convert \
  --gamp reads.gamp \
  --xg graph.xg \
  --gtf annotation.gtf \
  --collapse-manifest collapse.tsv \
  --out-dir out \
  --strand sense|antisense|both \
  [--assignment all|unique-transcript|unique-gene|starsolo-default] \
  [--score-window N] \
  [--min-splice-jump N] \
  [--max-traversals-per-read N] \
  [--tag-failures skip|fail] \
  [--tag-source auto|annotation|tags] \
  [--barcode-tag NAME --umi-tag NAME] \
  [--threads N]
```

## Required Inputs

- `--gamp`: name-grouped single-end GAMP from the same graph/node-id space as `--xg`.
- `--xg`: coordinate graph exposing visible source-coordinate paths through
  `PathPositionHandleGraph`.
- `--gtf`: GTF annotation using source path names as seqnames.
- `--collapse-manifest`: headered manifest keyed by
  `(source_path_name, source_transcript_id)`.
- `--out-dir`: output directory for `map.rad`, `tx2gene.tsv`, and run diagnostics.
- `--strand`: required; no default is allowed.

`.gcsa`, `.gcsa.lcp`, `.dist`, `.snarls`, minimizer indexes, GBZ, and GBWT are not V1
converter inputs after GAMP exists.

## Defaults And Validation

| Option | Default | Validation |
|---|---|---|
| `--assignment` | `all` | one of `all`, `unique-transcript`, `unique-gene`, `starsolo-default` |
| `--score-window` | `0` | integer `>= 0` |
| `--min-splice-jump` | `20` | integer `>= 1` |
| `--max-traversals-per-read` | `100000` | integer `>= 1` |
| `--tag-failures` | `skip` | `skip` or `fail` |
| `--tag-source` | `auto` | `auto`, `annotation`, or `tags` |
| `--threads` | implementation-defined default | integer `>= 1`; output must remain byte-identical across supported values |

If either `--barcode-tag` or `--umi-tag` is supplied, both must be supplied. Explicit tag
names are interpreted within the selected `--tag-source`; with `--tag-source auto`, both
direct annotations and SAM-style `tags` are searched for those names.

## Score Universe

`read_best_score` is the maximum target score among compatible canonical transcript
targets after source-to-canonical collapse. Incompatible traversals do not contribute to
`read_best_score`.

Score filtering can remove lower-scoring targets from an otherwise retained group. It
cannot by itself create a score-filtered-to-empty group because at least one compatible
target is the read-group best.

## Traversal Cap

The traversal cap applies to complete GAMP traversals attempted per read group before
compatibility filtering. If the next complete traversal would exceed
`--max-traversals-per-read`, the run fails hard with `traversal_cap_exceeded_groups`.

Cap overflow is not a skippable read-level condition in V1. No lossy partial target set is
valid unless a later decision approves one.

## Diagnostic Counters

The following counter names are stable Phase 1 test targets. Implementations may add more
counters, but these names must remain unambiguous.

| Counter | Meaning | Run status |
|---|---|---|
| `input_records` | GAMP records read | success or failure |
| `input_read_groups` | adjacent read-name groups started | success or failure |
| `emitted_groups` | read groups emitted to RAD | success |
| `tag_skipped_groups` | read groups skipped by default tag policy | success |
| `tag_non_coherent_groups` | corrected/raw/source tag state is not coherent | success unless strict |
| `tag_missing_groups` | no complete selected barcode/UMI pair | success unless strict |
| `tag_malformed_groups` | selected tag value cannot be encoded or has invalid type | success unless strict |
| `tag_mixed_length_groups` | selected CB or UMI length differs after global lengths are set | success unless strict |
| `tag_conflict_hard_failures` | records in one read group disagree on selected CB or UMI | hard failure |
| `grouping_recurrence_failures` | a completed read name appears again later | hard failure |
| `no_compatible_transcript_groups` | groups with no compatible transcript after projection | success |
| `score_removed_targets` | compatible targets removed by score-window filtering | success |
| `policy_removed_unique_transcript_groups` | groups removed by `unique-transcript` | success |
| `policy_removed_unique_gene_groups` | groups removed by `unique-gene` or `starsolo-default` | success |
| `manifest_missing_source_identities` | compatible source identities missing manifest rows | hard failure |
| `manifest_contradictory_rows` | one source identity maps to conflicting canonical targets | hard failure |
| `manifest_duplicate_rows` | repeated identical manifest rows | success |
| `manifest_gene_conflicts` | one canonical transcript maps to multiple genes | hard failure |
| `annotation_index_consistency_failures` | GTF/manifest source path absent from XG or otherwise inconsistent | hard failure |
| `annotation_manifest_mismatches` | manifest source transcript absent from GTF exon records | hard failure |
| `traversal_cap_exceeded_groups` | traversal enumeration cap exceeded | hard failure |

For hard failures, tests may assert either a structured summary emitted before abort or an
actionable error message containing the stable counter name and nonzero count. Successful
runs must include the relevant counters in the run summary.
