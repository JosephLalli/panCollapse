# Phase 1 Compatibility Fixtures

These fixtures specify VG-dependent projection and transcript-compatibility behavior.
They are conceptual until fixture-generation code is approved after Gate Behavior
Specified.

## Shared Synthetic Model

Coordinates are 0-based half-open path coordinates on visible source path `chrFixture`
unless stated otherwise.

| source transcript | canonical transcript | gene | strand | exons | implied introns |
|---|---|---|---|---|---|
| `SRC_A` | `TX_A` | `GENE_A` | `+` | `[100,150)`, `[200,250)`, `[300,350)` | `[150,200)`, `[250,300)` |
| `SRC_B` | `TX_B` | `GENE_A` | `+` | `[120,180)`, `[220,260)` | `[180,220)` |
| `SRC_C` | `TX_C` | `GENE_B` | `+` | `[500,550)`, `[600,650)` | `[550,600)` |
| `SRC_D` | `TX_D` | `GENE_C` | `-` | `[700,750)`, `[800,850)` | `[750,800)` |
| `SRC_E` | `TX_E` | `GENE_D` | `+` | `[1000,1050)`, `[1100,1150)` | `[1050,1100)` |

`GENE_D` has broader gene-level annotation through `[1250,1260)` with no transcript exon
or implied-intron model covering that interval. A read there must not make `SRC_E`
compatible by parent-gene overlap alone.

Copy/collapse fixtures may additionally use source path `altCopy` with source transcript
`SRC_A_COPY`, canonical transcript `TX_A`, gene `GENE_A`, and the same model as `SRC_A`.

Unless specified, fixtures use:

```text
--assignment all
--score-window 0
--tag-failures skip
--min-splice-jump 20
--strand sense
```

## Fixture Matrix

| ID | Behavior | Conceptual aligned evidence | Expected source identities | Expected canonical targets | Diagnostics |
|---|---|---|---|---|---|
| CMP-01 | Exonic compatibility | one block `[310,330)` on `chrFixture`, query-forward | `SRC_A` | `TX_A` | emitted; one target |
| CMP-02 | Intronic compatibility | one block `[160,170)` | `SRC_A` | `TX_A` | emitted; no splicing-state label |
| CMP-03 | Exon-to-intron boundary | one unspliced block `[140,170)` | `SRC_A` | `TX_A` | emitted |
| CMP-04 | Intron-to-exon boundary | one unspliced block `[190,210)` | `SRC_A` | `TX_A` | emitted |
| CMP-05 | Isoform exon/intron overlap | one block `[170,175)` | `SRC_A`, `SRC_B` | `TX_A`, `TX_B` | emitted; interval is intronic for `SRC_A` and exonic for `SRC_B` |
| CMP-06 | Annotated splice accepted | observed `connection` skip `[150,200)` with flanking aligned blocks in `SRC_A` exons | `SRC_A` | `TX_A` | emitted |
| CMP-07 | Absent splice rejected | observed `connection` skip `[150,200)` evaluated against `SRC_B` | none for `SRC_B` | no `TX_B` | `SRC_B` incompatible because junction is absent |
| CMP-08 | Short deletion not splice | two same-path blocks separated by non-connection gap `[145,155)`, gap length 10 | `SRC_A` | `TX_A` | emitted; gap is below `--min-splice-jump` |
| CMP-09 | Anchored outside overhang | one block `[90,115)` | `SRC_A` | `TX_A` | emitted because at least `[100,115)` overlaps model |
| CMP-10 | Parent-gene-only negative | one block `[1250,1260)` in `GENE_D` locus but outside `SRC_E` model | none | none | `no_compatible_transcript_groups++` |
| CMP-11 | Strand sense rejects opposite | block `[710,730)` on `SRC_D`, query-forward opposite transcript orientation, `--strand sense` | none | none | `no_compatible_transcript_groups++` |
| CMP-12 | Strand antisense accepts opposite | same evidence as CMP-11, `--strand antisense` | `SRC_D` | `TX_D` | emitted |
| CMP-13 | Strand both accepts either | same evidence as CMP-11, `--strand both` | `SRC_D` | `TX_D` | emitted |
| CMP-14 | Connection score and tie | two complete GAMP traversals: one compatible with `SRC_A`, one with `SRC_B`, both total score 30 after subpath plus connection scoring | `SRC_A`, `SRC_B` | `TX_A`, `TX_B` | tied-best retained |
| CMP-15 | Non-connection splice-length threshold | non-connection same-path gap `[150,200)`, default `--min-splice-jump 20` | `SRC_A` only if junction `[150,200)` is annotated | `TX_A` | treated as observed splice because gap length is 50 |
| CMP-16 | Long non-connection absent splice rejected | same gap `[150,200)` evaluated against `SRC_B`, whose annotated junction is `[180,220)` | none for `SRC_B` | no `TX_B` | `SRC_B` incompatible because observed junction is absent |
| CMP-17 | Annotation/index path mismatch | GTF/manifest use `chrFixture_missing`, absent from `.xg` visible paths | none | none | hard failure: `annotation_index_consistency_failures++` |
| CMP-18 | Missing manifest coverage | evidence compatible with `(chrFixture, SRC_A)` but manifest lacks that row | none | none | hard failure: `manifest_missing_source_identities++`, no identity fallback |

## Traversal Enumeration Safety

| ID | Behavior | Conceptual GAMP DAG | Options | Expected result | Diagnostics |
|---|---|---|---|---|---|
| TRV-01 | Within traversal cap | DAG with exactly four complete traversals | `--max-traversals-per-read 4` | normal compatibility evaluation continues | no cap diagnostic |
| TRV-02 | Traversal cap overflow | same DAG plus one more complete traversal | `--max-traversals-per-read 4` | hard failure; no partial target set is emitted | `traversal_cap_exceeded_groups++` |

## Required Assertions

- Project every reference-consuming edit through `PathPositionHandleGraph`; do not use
  nearest-path search.
- Preserve multiple source-path projections before collapse.
- Evaluate compatibility before source-to-canonical collapse.
- Require at least one aligned reference-consuming base to overlap the candidate
  transcript exon-or-implied-intron model.
- Treat parent-gene overlap alone as insufficient.
- Sort and deduplicate retained canonical targets before RAD writing.
