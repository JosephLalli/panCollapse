# PathTally Test Plan

Verification plan for the GAMP-to-RAD algorithm in `docs/conversion-algorithm.md` (D048),
named **PathTally**: each aligned node casts its vg score into a per-HST-path tally, and the
winning tallies collapse to transcripts. One test group per implementation increment.

## 1. Per-node vg score

- Unit: hand-built subpaths covering match, mismatch, insertion, deletion, a gap spanning a
  node boundary, a read-end (full-length bonus), and a soft-clip; assert each node's computed
  score equals a hand-worked value.
- Oracle: over every subpath in a real GAMP (e.g. `build/real_mhc_scratch/mhcA.gamp`), assert
  `sum(per-node scores) == Subpath.score`. A nonzero mismatch count means either a bug or that
  quality-adjusted scoring was on; report which.
- Status: PASS (`tests/vg/pathtally_score_test.cpp`). Synthetic unit asserts hold exactly. On
  `mhcA.gamp`, 93.1% of subpaths reproduce `Subpath.score` exactly; the rest are
  quality-adjusted lower-quality bases (mean q 22.4 vs 34.4). Flat score adopted as the
  per-node weight. The committed CTest runs the synthetic asserts only (the real GAMP is
  gitignored scratch); the real-GAMP check is self-validating when the file is passed.
- Optional quality-adjusted scorer: `--score qualadj` reproduces vg's QualAdjAligner
  (`src/pathtally_qualadj.hpp`; recovered log-base, quality-indexed log-odds matrix, and
  per-base scoring). It matches `Subpath.score` for 99.2% of subpaths on `mhcA.gamp`; the
  residual +/-1 is floating-point rounding at score-matrix boundaries.

## 2. Node-to-HST attribution and collapse

- Tiny-graph unit: a read touching a node on two HST paths tallies both; stripping
  `_H<n>` / `_R<n>` yields the right transcript IDs; two haplotype copies of one transcript
  collapse to one ID without doubling the tally.
- Status: PASS (`tests/vg/pathtally_pipeline_test.cpp`, synthetic alignments + injected lookup).

## 3. Winner selection

- Unit: a multi-alignment read whose top HST is in a supplementary record — assert it wins; a
  lower-scoring HST is excluded; exact ties across alignments are all retained.
- Status: PASS (`pathtally_pipeline_test.cpp` cases D and E).

## 4. Orientation

- Unit: forward read -> `dirs` forward; reverse read -> reverse; a transcript whose winning
  HSTs split forward/reverse resolves to the majority-of-aligned-bases side, with the forward
  fallback on an exact base tie.
- Status: PASS (`pathtally_pipeline_test.cpp` case B).

## 5. RAD emission and interop

- Byte-level: assert `map.rad` header/tags/records and `tx2gene.tsv` for a fixed tiny input.
- Interop: run `alevin-fry generate-permit-list` / `collate` / `quant` and assert the exact
  cell-by-gene matrix.
- Determinism: re-run and assert byte-identical output.
- Status: interop DONE on real data. `src/main.cpp` is reimplemented on PathTally (inputs
  `--gamp/--xg/--t2g`, plus `--score flat|qualadj`); it converted the real 1M-read MHC GAMP to
  a valid `map.rad` (39,103 records), `tx2gene.tsv` (1022 transcripts), and summary, and
  alevin-fry 0.15.0 consumed that RAD through `generate-permit-list` (46 corrected barcodes),
  `collate` (257 chunks), and `quant` to a 257-cell x 317-gene count matrix. A committed
  tiny byte-level fixture and the retirement of the old traversal-era CTest fixtures remain.

## 6. Human-pangenome fixture

- End-to-end (`docs/testing_fixture_creation.md`): MHC slice -> `vg rna` -> simulate -> GAMP
  -> PathTally RAD, compared exactly (target sets, `dirs`, CB/UMI, counters) against the
  independent GAMP-driven oracle that re-implements steps 1-4.
- Status: PASS. `tests/vg/pathtally_oracle.py` independently reimplements steps 1-4 (per-node
  scoring, node-to-HST attribution from the graph P-lines, winner selection, collapse,
  orientation) without using any PathTally code. On a 10,551-read-group sample of the real
  MHC pangenome GAMP, panCollapse's RAD matched the oracle exactly for all 420 emitted records
  (target sets, orientation, packed CB/UMI). The oracle/comparator plus a committed tiny
  fixture (`tests/vg/fixtures/pathtally_smoke/`) run as a self-contained CTest
  (`pathtally_smoke_oracle`); the large real-MHC comparison stays a manual run on scratch data.
- Status: PASS on deterministic `vg sim` reads too. `vg sim -x spliced.xg -P <HST...> -n 400
  -l 90 -s 1 -q` produces spliced reads from four chosen HST paths; `tests/vg/fixture_tag_reads.py`
  appends synthetic CB/UMI; `vg mpmap -F GAMP` maps them; panCollapse emits RAD for all 400
  (byte-identical on re-run), the four source transcripts appear in the dictionary, and the
  independent oracle matches all 400 records exactly.
