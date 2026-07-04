# Validation Contract

Validation is organized around observable behavior. The GAMP-to-RAD algorithm is defined by
D048 and `docs/conversion-algorithm.md`.

## 1. Workspace and planning gates

- external API claims have primary-source evidence;
- Phase 0 research briefs are complete;
- fixture expectations exist before broad implementation.

## 2. Barcode and UMI cases

- RNA GAMP name follows `<original_read_name>_<raw_CB>_<raw_UMI>`, parsed from the rightmost
  two underscore-delimited fields (the original name may contain underscores);
- extracted values are written to RAD without correction;
- parsed values must match the configured `--raw-cb-length` / `--raw-umi-length` (Phase 2
  defaults 16 and 12);
- panCollapse does not permit-list, correct, or deduplicate; alevin-fry does that;
- `--molecule-identity-failures skip|fail` defaults to `skip`; missing, malformed,
  unsupported, or length-mismatched values are skipped and counted by default and fatal
  under `fail`.

## 3. Grouping cases

- one record for one read; several adjacent records with the same name; next read begins
  normally;
- a completed read name recurs later: hard failure;
- empty input and truncated/corrupt stream: defined behavior.

## 4. Scoring cases

- per-node scores reproduce vg's scheme: on real GAMP subpaths, the sum of a subpath's
  per-node scores equals `Subpath.score`;
- a match/mismatch/gap mix on one node scores as vg does (match +1/base, mismatch -4/base,
  affine gaps open 6 / extend 1, read-end bonus +5);
- an affine gap spanning a node boundary is charged gap-open once;
- base-quality-adjusted input is detectable when the flat per-node sum does not equal
  `Subpath.score`.

## 5. Compatibility and collapse cases

- a read whose aligned nodes lie on one HST path is compatible with that transcript;
- a read whose aligned nodes lie on no HST path emits no target and is counted;
- a node shared by two isoforms' HST paths makes the read compatible with both;
- several HST paths of one transcript (haplotype copies) collapse to one transcript ID and
  do not inflate the transcript's score;
- the winners are the HSTs tied at the single top score pooled across all of the read's
  alignments; lower-scoring HSTs are not emitted;
- a read with a supplementary alignment contributes its winning HSTs to the same pooled
  top-score selection.

## 6. Orientation cases

- read running along an HST path: RAD `dirs` records forward;
- read running opposite: RAD `dirs` records reverse;
- a transcript whose winning HSTs disagree: the majority of aligned bases decides, with a
  deterministic forward fallback for an exact tie.

## 7. RAD interoperability

A tiny fixture must prove:

1. panCollapse output is accepted by the supported alevin-fry version;
2. permit-list generation and collation complete;
3. quantification with the two-column t2g completes;
4. the final cell-by-gene matrix matches expected UMI counts;
5. RAD `cblen`/`ulen` match the configured lengths (Phase 2: 16, 12);
6. no USA/splicing-state rows;
7. Phase 2 uses one thread; D045 defers multithreading; any future mode must still prove
   byte-identical output;
8. streaming-to-disk framing (chunk headers and file-level `num_chunks` seek-and-backpatched
   to their final values, D049) decodes through the supported libradicl/alevin-fry path,
   including a forced multi-chunk split.

## 8. Failure and diagnostics

Each hard failure has a stable nonzero exit status and actionable message. Summary counts
are independently checkable against fixtures.

## 9. Production-applicability fixture

- `docs/testing_fixture_creation.md` is the human-pangenome GAMP-to-RAD fixture plan (D047):
  slice the HPRC v1.1 GRCh38 graph over the MHC region, build the spliced/HST graph with
  `vg rna`, simulate reads, map to GAMP, and compare panCollapse RAD against an independent
  GAMP-driven oracle that re-implements the D048 algorithm;
- record throughput, peak memory, and target-set size distribution;
- evaluate direct `vg mpmap` to panCollapse stdin streaming while keeping RAD output on the
  streaming-to-disk path;
- do not add a custom index unless the measured lookup cost is material and a human approves.
