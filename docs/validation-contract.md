# Validation Contract

Validation is organized around observable behavior rather than implementation internals.
Exact fixture encodings are designed after Gate Architecture Approved, but the required cases are
settled now.

Phase 1 fixture expectations are specified in focused documents under `docs/phase1/`.
Use `docs/phase1/README.md` to choose the minimum document bundle for each implementation
or review task.

## 1. Workspace and planning gates

Before implementation:

- external API claims have primary-source evidence;
- all Phase 0 research briefs are complete;
- architecture proposal identifies exact required inputs and dependencies;
- no production code exists before passing Gate Architecture Approved;
- fixture expectations exist before broad implementation.

## 2. Barcode and UMI cases

- RNA GAMP name field follows `<original_read_name>_<raw_CB>_<raw_UMI>`.
- parsing uses the rightmost two underscore-delimited fields for raw CB and raw UMI, so
  the original read name may contain underscores.
- extracted values are the observed raw values and are written to RAD without correction.
- raw CB and UMI lengths are explicit CLI-controlled values; parsed values must match
  those configured lengths.
- panCollapse does not perform permit-list construction, cell-barcode correction, or UMI
  deduplication/resolution; alevin-fry performs those steps downstream.
- missing CB or UMI field: skip and count by default; fail in strict mode.
- malformed or unsupported CB/UMI value: skip and count by default; fail in strict mode.
- raw CB or UMI length mismatch against the configured lengths: skip and count by
  default; fail in strict mode.

## 3. Grouping cases

- one record for one read;
- several adjacent records with same read name;
- next read begins normally;
- completed read name recurs later: hard failure;
- empty input and truncated/corrupt stream: defined behavior.

## 4. Compatibility cases

Every row in `docs/compatibility-semantics.md` is required. Include overlapping isoforms,
annotated and unannotated splice junctions, both orientations, the explicit
outside-first/last-exon extension case with a positive transcript-model anchor, and a
negative case where only the parent gene locus overlaps.

## 5. Score and collapse cases

- one compatible target;
- two tied canonical targets;
- lower-scoring target removed by default;
- lower-scoring target retained by configured score window;
- two source transcript identities collapse to one canonical transcript and use max score;
- graph-path multiplicity does not inflate target score;
- score filtering removes lower targets but cannot by itself create an empty group;
- traversal cap within-limit and overflow cases;
- missing source transcript identity in manifest is a hard failure with no identity
  fallback;
- contradictory manifest rows fail.
- one canonical transcript mapped to multiple genes fails.

## 6. Assignment-policy cases

- `all`: retain complete eligible transcript set across multiple genes.
- `unique-transcript`: one transcript passes, more than one does not.
- `unique-gene`: multiple transcripts from one gene pass and all remain in RAD.
- `unique-gene`: transcripts from two genes do not pass.
- `starsolo-default`: exactly matches `unique-gene` for every fixture.

## 7. RAD interoperability

A tiny fixture must prove:

1. panCollapse output is accepted by the supported alevin-fry version.
2. permit-list generation and collation complete.
3. quantification with a two-column transcript-to-gene map completes.
4. the final exact cell-by-gene matrix matches the expected UMI counts.
5. RAD `cblen` and `ulen` match the configured raw CB and UMI lengths.
6. no USA/splicing-state rows are emitted in V1.
7. Phase 2 uses one thread; repeated runs across supported thread counts are a Phase 3
   and final V1 acceptance requirement.

## 8. Failure and diagnostics

Each hard failure has a stable nonzero exit status and actionable message. Summary counts
must be independently checkable against fixtures.

## 9. Performance pilot

After correctness:

- define a bounded real-data slice;
- record throughput, peak memory, target-set size distribution, and annotation-lookup
  cost;
- compare one versus multiple threads if threading is implemented;
- do not add a custom index unless the measured lookup cost is material and a human
  approves the design change.
