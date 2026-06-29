# Phase 1 Policy Fixtures

These fixtures are pure policy contracts. They should be testable without VG headers,
GAMP parsing, GTF parsing, or XG projection by supplying candidate source identities,
scores, tags, and manifest rows directly.

## Shared Canonical Map

```text
TX_A -> GENE_A
TX_B -> GENE_A
TX_C -> GENE_B
TX_D -> GENE_C
```

## Score And Collapse

`read_best_score` is computed only from compatible canonical transcript targets after
source-to-canonical collapse. Incompatible traversals do not contribute to
`read_best_score`.

| ID | Behavior | Candidate input after compatibility | Options | Expected targets | Diagnostics |
|---|---|---|---|---|---|
| SC-01 | One compatible target | `TX_A=40` | default | `TX_A` | emitted, one target |
| SC-02 | Tied best | `TX_A=40`, `TX_C=40` | `--score-window 0` | `TX_A`, `TX_C` | both retained |
| SC-03 | Lower score removed | `TX_A=40`, `TX_C=37` | `--score-window 0` | `TX_A` | `score_removed_targets=1` for `TX_C` |
| SC-04 | Score window retains lower target | `TX_A=40`, `TX_C=37` | `--score-window 3` | `TX_A`, `TX_C` | both retained |
| COL-01 | Copy collapse uses max score | `(chrFixture,SRC_A)->TX_A score 30`, `(altCopy,SRC_A_COPY)->TX_A score 35` | default | `TX_A` score 35 | no score summing |
| COL-02 | Multiplicity does not inflate | two equivalent source paths to `TX_A` score 30 each; `TX_C` score 50 | default | `TX_C` | `TX_A` remains 30, not 60 |
| ORD-01 | Target order and dedup | retained targets arrive `TX_B`, `TX_A`, `TX_A` | default | `TX_A`, `TX_B` | sorted/deduplicated |

## Assignment Policies

Eligibility filtering and collapse happen before these policies.

| ID | Behavior | Eligible targets after score/collapse | Assignment | Expected targets | Diagnostics |
|---|---|---|---|---|---|
| POL-01 | `all` preserves multi-gene set | `TX_A:GENE_A`, `TX_C:GENE_B` | `all` | `TX_A`, `TX_C` | emitted |
| POL-02 | `unique-transcript` accepts one | `TX_A:GENE_A` | `unique-transcript` | `TX_A` | emitted |
| POL-03 | `unique-transcript` rejects many | `TX_A:GENE_A`, `TX_B:GENE_A` | `unique-transcript` | none | policy removal |
| POL-04 | `unique-gene` preserves same-gene transcripts | `TX_A:GENE_A`, `TX_B:GENE_A` | `unique-gene` | `TX_A`, `TX_B` | emitted |
| POL-05 | `unique-gene` rejects multi-gene set | `TX_A:GENE_A`, `TX_C:GENE_B` | `unique-gene` | none | policy removal |
| POL-06 | `starsolo-default` aliases `unique-gene` | all POL-04 and POL-05 inputs | `starsolo-default` | exactly the `unique-gene` result | no divergence allowed |
| POL-07 | Score before uniqueness | `TX_A:GENE_A=50`, `TX_C:GENE_B=45` | `unique-gene`, window 0 | `TX_A` | lower gene removed before policy |
| POL-08 | Window before uniqueness can create ambiguity | same as POL-07 | `unique-gene`, window 5 | none | policy removal after window retains both genes |

## Diagnostics Summary Fixture

| ID | Input groups | Expected summary |
|---|---|---|
| DIA-01 | one emitted one-target group; one emitted two-target group; one emitted score-filtered group with one lower target removed; one tag skip; one no-compatible group; one `unique-transcript` policy drop; one `unique-gene` policy drop | `input_read_groups=7`, `emitted_groups=3`, `tag_skipped_groups=1`, `no_compatible_transcript_groups=1`, `score_removed_targets=1`, `policy_removed_unique_transcript_groups=1`, `policy_removed_unique_gene_groups=1`, target-count histogram `{1:2, 2:1}` |

## Required Assertions

- Scores are never summed across source copies, haplotypes, or equivalent paths.
- Default score window is 0 and retains tied best targets.
- Score filtering removes targets, not whole groups; at least one compatible target is the
  read-group best.
- Policy removals happen after score filtering and collapse.
- `starsolo-default` is exactly `unique-gene` in V1.
- Output target order is stable and lexicographic by canonical transcript ID.
