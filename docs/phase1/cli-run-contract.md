# Phase 1 CLI And Run Contract

This document is the Phase 1 CLI snapshot required for **Gate Behavior Specified**. It
collects the user-facing run contract without forcing fixture implementers to load every
Phase 1 fixture document.

Supersession note: D038 supersedes the barcode/UMI tag-source portions of this Phase 1
snapshot for V1 implementation. panCollapse reads observed raw CB/UMI from the GAMP name
field; corrected/raw GAMP annotation tag selection and tag-name overrides are historical
Phase 1 text unless a later human-approved decision restores them.

Supersession note: D042 removes panCollapse-side `--strand` filtering from the active V1
interface. panCollapse preserves target-relative orientation in RAD `dirs`; downstream
alevin-fry expected-orientation settings handle library-orientation filtering.

Supersession note: D043 replaces historical tag-failure diagnostics for active V1 raw
molecule identity. Use `--molecule-identity-failures skip|fail` and the
`raw_molecule_*` counters for GAMP-name CB/UMI parsing.

Supersession note: D044 narrows active GAMP-to-RAD assignment behavior to `all` only.
`unique-transcript`, `unique-gene`, and `starsolo-default` are to-be-implemented future
options outside the active RAD converter, and their historical policy-removal counters
are not active V1 RAD-output requirements.

## Command Snapshot

```text
panCollapse convert \
  --gamp reads.gamp \
  --xg graph.xg \
  --gtf annotation.gtf \
  --collapse-manifest collapse.tsv \
  --out-dir out \
  [--assignment all] \
  [--score-window N] \
  [--min-splice-jump N] \
  [--max-traversals-per-read N] \
  [--molecule-identity-failures skip|fail]
```

D045 defers panCollapse-side multithreading; `--threads` is not an active production CLI
surface unless a later human-approved decision restores it.

## Required Inputs

- `--gamp`: name-grouped single-end GAMP from the same graph/node-id space as `--xg`.
- `--xg`: coordinate graph exposing visible source-coordinate paths through
  `PathPositionHandleGraph`.
- `--gtf`: GTF annotation using source path names as seqnames.
- `--collapse-manifest`: headered manifest keyed by
  `(source_path_name, source_transcript_id)`.
- `--out-dir`: output directory for `map.rad`, `tx2gene.tsv`, and run diagnostics.

`.gcsa`, `.gcsa.lcp`, `.dist`, `.snarls`, minimizer indexes, GBZ, and GBWT are not V1
converter inputs after GAMP exists.

## Defaults And Validation

| Option | Default | Validation |
|---|---|---|
| `--assignment` | `all` | active RAD converter accepts `all`; `unique-transcript`, `unique-gene`, and `starsolo-default` fail as to-be-implemented |
| `--score-window` | `0` | integer `>= 0` |
| `--min-splice-jump` | `20` | integer `>= 1` |
| `--max-traversals-per-read` | `100000` | integer `>= 1` |
| `--molecule-identity-failures` | `skip` | `skip` or `fail` |

Historical Phase 1 tag-selection options are superseded by D038 and D043; they are not
part of the active V1 CLI unless a later human-approved decision restores them.

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
| `raw_molecule_skipped_groups` | read groups skipped by active V1 raw-name molecule-identity policy | success |
| `raw_molecule_missing_groups` | no complete raw CB/UMI pair can be parsed from the GAMP name | success unless strict |
| `raw_molecule_malformed_groups` | raw CB/UMI schema or configured length check fails | success unless strict |
| `raw_molecule_unsupported_groups` | raw CB/UMI contains an unsupported base for RAD encoding | success unless strict |
| `grouping_recurrence_failures` | a completed read name appears again later | hard failure |
| `no_compatible_transcript_groups` | groups with no compatible transcript after projection | success |
| `mixed_orientation_dropped_groups` | groups dropped because an emitted target has mixed target-relative orientations | success |
| `score_removed_targets` | compatible targets removed by score-window filtering | success |
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
