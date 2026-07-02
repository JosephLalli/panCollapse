> **Superseded by D048.** The active GAMP-to-RAD algorithm is `docs/conversion-algorithm.md`.
> D048 removes the collapse manifest from runtime inputs; the manifest validation fixtures
> (`MAN-01` through `MAN-08`), the `manifest_*` counters, and
> `annotation_index_consistency_failures` and `annotation_manifest_mismatches` are from the
> pre-D048 design and are not current acceptance criteria. Raw CB/UMI grouping, molecule-
> identity failure modes (`GRP-*`, `TAG-*` as historical; D038/D043/D046 govern active V1),
> and recurrence validation remain directionally relevant. These Phase 1 fixtures are
> preserved as historical context.

# Phase 1 Input And Diagnostic Fixtures

These fixtures cover tag selection, grouping validation, manifest validation, and failure
accounting. They are split by whether they can be exercised as pure policy or need GAMP/XG
fixtures.

Supersession note: the barcode/UMI tag-source cases in this document are historical
Phase 1 contract text. D038 supersedes them for V1 implementation: panCollapse reads
observed raw CB/UMI from the GAMP name field, not corrected/raw GAMP annotation tags.
D043 sets the active raw-name failure mode to
`--molecule-identity-failures skip|fail` with `raw_molecule_*` counters.

## Barcode And UMI

Unless stated, a compatible target `TX_A` is otherwise available.

| ID | Behavior | Input setup | Expected output | Diagnostics |
|---|---|---|---|---|
| TAG-01 | Corrected tags selected | coherent `CB+UB` | `TX_A` emitted with corrected CB/UMI | selected corrected pair |
| TAG-02 | Corrected preferred over raw | coherent `CB+UB` and `CR+UR` in same supported source | `TX_A` emitted with corrected values | raw disagreement counted if values differ |
| TAG-03 | Corrected in one source beats raw in another | direct annotation has coherent `CB+UB`; SAM-style `tags` has coherent `CR+UR` | `TX_A` emitted with corrected values | proves cross-source corrected precedence |
| TAG-04 | Raw fallback | only coherent `CR+UR` | `TX_A` emitted with raw values | selected raw pair |
| TAG-05 | Source conflict | direct `CB` and SAM-style `tags` `CB` disagree | skipped by default | `tag_non_coherent_groups++`; strict mode hard failure |
| TAG-06 | Mixed pair rejected | `CB+UR`, no coherent pair | skipped by default | `tag_non_coherent_groups++`; strict mode hard failure |
| TAG-07 | Missing mate rejected | `CB` without `UB`; or `UR` without `CR` | skipped by default | `tag_missing_groups++`; strict mode hard failure |
| TAG-08 | Malformed value rejected | non-string tag, unsupported base character if not normalized, or CB/UMI length >32 | skipped by default | `tag_malformed_groups++`; strict mode hard failure |
| TAG-09 | Mixed selected lengths | first emitted group establishes 16 bp CB and 10 bp UMI; later selected value has 15 bp CB or 11 bp UMI | later group skipped by default | `tag_mixed_length_groups++`; strict mode hard failure |
| TAG-10 | Intra-group conflict | adjacent records with one read name disagree on selected CB or UMI | hard failure | `tag_conflict_hard_failures++` |

## Grouping And Ordering

| ID | Behavior | Input stream | Expected output | Diagnostics |
|---|---|---|---|---|
| GRP-01 | Single read group | `[R1]` | one emitted group if compatible | `input_records=1`, `input_read_groups=1` |
| GRP-02 | Adjacent alternatives | `[R1a,R1b] [R2]`; `R1a/R1b` tied | `R1` emits union target set in input group position, then `R2` | group order preserved |
| GRP-03 | Name recurrence | `[R1] [R2] [R1]` | hard failure | `grouping_recurrence_failures++` |
| GRP-04 | Empty input | empty GAMP stream | success, no records | all counts zero except version/config metadata |
| GRP-05 | Truncated/corrupt stream | invalid GAMP bytes | hard failure | unreadable/corrupt input |

## Manifest And Identity

| ID | Behavior | Input setup | Expected result | Diagnostics |
|---|---|---|---|---|
| MAN-01 | Explicit identity mapping | `(chrFixture,SRC_A)->TX_A,GENE_A` | accepted | source identity available |
| MAN-02 | Missing compatible identity | compatible `(chrFixture,SRC_A)` lacks manifest row | hard failure | `manifest_missing_source_identities++`; no identity fallback |
| MAN-03 | Partial manifest coverage is fatal | one read is compatible with covered `(chrFixture,SRC_A)` and uncovered `(chrFixture,SRC_B)` | hard failure | `manifest_missing_source_identities++`; must not silently emit only `TX_A` |
| MAN-04 | Contradictory duplicate | same `(source_path_name,source_transcript_id)` maps to two canonical transcripts | hard failure at manifest load | `manifest_contradictory_rows++` |
| MAN-05 | Repeated identical row | same row repeated exactly | accepted with diagnostic-only duplicate count | `manifest_duplicate_rows++`; no behavior change |
| MAN-06 | Canonical transcript maps to two genes | `TX_A->GENE_A` and `TX_A->GENE_B` | hard failure at manifest load | `manifest_gene_conflicts++` |
| MAN-07 | Missing visible path | manifest/GTF path absent from `.xg` | hard failure | `annotation_index_consistency_failures++` |
| MAN-08 | Source transcript missing in GTF | manifest source transcript absent from GTF exon records | hard failure | `annotation_manifest_mismatches++` |

## Required Assertions

- Do not combine corrected and raw tags into a synthetic pair.
- Do not silently repair malformed CB/UMI values.
- Completed read names must not recur later.
- Manifest is applied before RAD target emission.
- Missing source identity coverage is fatal, not a skipped read.
- Every hard failure has a stable nonzero exit status and an actionable message.
- Hard-failure tests assert the stable counter name either in a pre-abort summary or in
  the fatal diagnostic message.
