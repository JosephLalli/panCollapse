# Progress

## Current phase

**Phase 2 thin vertical implementation is complete and locally verified.**

Production source now exists for the approved Phase 2 vertical slice. No public
panCollapse headers or checked-in generated fixtures, GAMP, XG, GCSA, distance-index,
GBZ, or RAD outputs have been created; generated graph/RAD artifacts remain confined to
ignored build directories.

## Settled product decisions

- Project name: `panCollapse`.
- C++20, GCC 15 environment, CMake + Ninja.
- Standalone project linking to an existing VG installation or checkout.
- GTF annotation; no GFF3 requirement in V1.
- V1 uses preexisting VG/index files and does not build a custom lookup index.
- Input GAMP must be grouped by read name; the tool validates this contract.
- CB/UMI come from the GAMP name field as observed raw values prepared upstream.
  panCollapse does not correct cell barcodes or UMIs; alevin-fry performs permit-list
  construction/cell-barcode correction and UMI deduplication/resolution downstream.
- Raw CB and UMI lengths are CLI-controlled values. Phase 2 defaults are
  `--raw-cb-length 16` and `--raw-umi-length 12`.
- Intronic and exon–intron-boundary evidence can make a transcript compatible.
- Strand policy is selectable: sense, antisense, or both.
- Copy/path collapse uses an explicit deterministic manifest.
- Phase 0 resolved the manifest source key as
  `(source_path_name, source_transcript_id)`.
- Phase 0 recentered V1 around mapper-side availability of `graph.xg`, `graph.gcsa`, and
  `graph.dist`: panCollapse consumes GAMP plus the matching `.xg`; `.gcsa`/`.dist` remain
  upstream `vg mpmap` artifacts and are not converter inputs or V1 provenance flags.
- Tied-best target score is the default; a nonzero score window is an advanced option.
- Full transcript equivalence classes are retained by default.
- V1 emits mapper-style uncollated RAD for standard alevin-fry processing.
- V1 does not classify spliced versus unspliced molecules.
- Transcript overhang is allowed only when at least one aligned base overlaps that
  transcript's exon/intron model.
- Missing, malformed, or unsupported raw CB/UMI values parsed from the GAMP name field
  are skipped and counted by default, with a strict failure mode.
- Collapse-manifest coverage is mandatory and explicit.
- `starsolo-default` is an exact alias for post-collapse `unique-gene`.
- Final V1 output must be byte-identical across supported thread counts. Phase 2 is
  single-threaded; multithreading and thread-count byte comparison are deferred to
  Phase 3.
- License: Apache-2.0.

## Next action

Begin Phase 3 planning/work only after choosing one independently testable behavior and
loading the relevant contracts.

## Required stop

Do not broaden beyond the approved Phase 2 vertical slice until Gate Vertical Slice
Proven is reviewed. Phase 3 work must proceed one independently testable behavior at a
time.

## Phase 0 artifacts

- `docs/research/vg-integration.md` — local VG v1.75.0-68-ge82694b69 evidence, GAMP API,
  upstream `mpmap` artifact context, XG/path-position boundary, annotation access, VG
  API wiki review, and smoke-validated link boundary.
- `docs/research/gamp-grouping-and-sorting.md` — version-scoped single-end GAMP grouping
  verdict and no-`gamsort` remedy.
- `docs/research/rad-format.md` — alevin-fry v0.15.0/libradicl v0.13.0 RAD wire schema,
  native writer strategy, and conformance plan.
- `docs/research/annotation-lookup-performance.md` — source transcript identity, GTF/XG
  projection design, strand/splice rules, and no-custom-index thresholds.
- `docs/architecture-proposal.md` — proposed V1 inputs, CLI, data flow, dependency
  boundary, deterministic writer model, diagnostics, alternatives, and Phase 1 plan.
- `docs/decisions.md` — proposed D023-D034 resolving Phase 0 forks and superseding broad
  VG/path-only wording.

## Phase 1 artifacts

- `docs/phase1/README.md` — routing index for future agents and reviewers.
- `docs/phase1/cli-run-contract.md` — focused CLI, option-default, exit, traversal-cap,
  and stable diagnostic-counter contract.
- `docs/phase1/compatibility-fixtures.md` — VG-dependent compatibility and projection
  fixture expectations.
- `docs/phase1/policy-fixtures.md` — pure score, collapse, assignment, target ordering,
  and summary fixtures.
- `docs/phase1/input-diagnostics-fixtures.md` — barcode/UMI, grouping, manifest, and
  diagnostic fixtures.
- `docs/phase1/rad-interop-fixture.md` — tiny RAD/alevin-fry interoperability fixture
  contract and exact expected cell-by-gene matrix.
- `docs/phase1/build-test-plan.md` — CMake/Ninja/CTest skeleton contract and verification
  commands.
- `CMakeLists.txt`, `cmake/PanCollapseVg.cmake`, and `tests/` — Phase 1 build/test
  skeleton with one pure policy smoke, one VG link smoke, and one build-dir-only
  GAMP/XG/GTF projection smoke.

## Phase 2 planning artifacts

- `docs/phase2/implementation-plan.md` — focused vertical-slice plan using GAMP plus the
  matching `.xg`, build-dir-only fixtures, raw read-name CB/UMI extraction, single-thread
  execution, configured raw CB/UMI lengths, two-column `tx2gene.tsv`, and a 1-cell x
  1-gene alevin-fry assertion. It also records the user-approved Phase 2 strand scope:
  `--strand sense` is implemented, while `antisense` and `both` emit to-be-implemented
  errors and are deferred to Phase 3 research and implementation.
- RAD docs now explicitly state that production writing is native C++, while libradicl
  and alevin-fry are validation oracles. They also document per-record `bc`, `umi`,
  `refs`, and `dirs` semantics.

## Phase 2 implementation artifacts

- `src/main.cpp` and `src/CMakeLists.txt` — minimal VG-backed `panCollapse convert`
  executable for the approved one-read vertical slice.
- `tests/vg/phase2_fixture_writer.cpp` — build-dir-only GFA, GTF, collapse manifest, and
  JSON GAMP fixture generator.
- `tests/vg/phase2_quant_verify.py` — strict Phase 2 verifier for `tx2gene.tsv`, RAD
  header/file tags, packed raw CB/UMI, `refs=[TX_A_ID]`, synthetic-forward `dirs`, and
  the final 1-cell x 1-gene alevin-fry matrix.
- `tests/vg/phase2_expect_strand_tbi.sh` — negative check that `--strand antisense` and
  `--strand both` fail with a Phase 3 to-be-implemented error in Phase 2.
- `tests/vg/CMakeLists.txt` — CTest chain for fixture generation, local `vg` conversion
  to `.xg` and binary GAMP, primary and repeat single-thread `panCollapse convert` runs,
  byte-for-byte output comparison, alevin-fry permit-list/collate/quant, and strict
  output verification.

## Phase 0 verification

- Original read-only research agents completed for VG integration, GAMP grouping, RAD
  format, and annotation semantics.
- Imported `.claude/agents/oracle.md` from the adjacent reviewer pattern and used it for
  skeptical review. Its first pass found blocking gaps; follow-up research resolved them.
- Throwaway VG link smoke outside the repo succeeded with `VGio::VGio`,
  `libhandlegraph::handlegraph_shared`, imported `libxg.a`, SDSL,
  divsufsort/divsufsort64, OpenMP, `pthread`, `m`, and `atomic`.
- Reviewed the VG `mpmap` wiki and VG API programming wiki. The docs now treat
  `graph.xg`, `graph.gcsa`, and `graph.dist` as expected upstream mapper artifacts, keep
  `.xg` as the V1 coordinate input, and keep GBZ/GBWT as a future alternate requiring
  separate projection proof.
- At Phase 0 close, checked no production/build/fixture/generated files existed with:
  `find . -maxdepth 3 \( -name CMakeLists.txt -o -path './src/*' -o -path './include/*' -o -path './tests/*' -o -name '*.rad' -o -name map.rad -o -name '*.gamp' -o -name '*.xg' -o -name '*.gcsa' -o -name '*.dist' -o -name '*.gbz' \)`.
- Direct stale-term audit found no remaining `not started`, path-only manifest, source
  order tag-precedence, or unresolved Phase 0 placeholder language outside historical
  decision text explicitly superseded by D032-D034.

## Phase 1 verification

- Delegated focused read-only reviews for the synthetic fixture matrix, RAD
  interoperability fixture, and build/test skeleton.
- Created focused Phase 1 contract documents instead of one monolithic contract, so future
  agents can load only the relevant task bundle.
- Oracle review found Phase 1 blockers in score-removal diagnostics, traversal-cap
  fixture coverage, and CLI routing; the contracts were revised to resolve those issues.
- Final oracle cleanup found an ambiguous parent-gene-only fixture and stale entry-point
  verification text; `CMP-10`, `README.md`, `.gitignore`, and
  `scripts/verify-workspace.sh` were revised.
- Pure build/test passed:
  `cmake -S . -B build-pure -G Ninja -DPANCOLLAPSE_ENABLE_VG_TESTS=OFF`;
  `cmake --build build-pure`;
  `ctest --test-dir build-pure -L pure --output-on-failure`.
- Full local VG smoke passed:
  `PKG_CONFIG_PATH=/mnt/ssd/lalli/usr/local/src/vg/lib/pkgconfig cmake -S . -B build -G Ninja -DPANCOLLAPSE_VG_ROOT=/mnt/ssd/lalli/usr/local/src/vg -DCMAKE_PREFIX_PATH=/mnt/ssd/lalli/usr/local/src/vg -DCMAKE_BUILD_RPATH=/mnt/ssd/lalli/usr/local/src/vg/lib`;
  `cmake --build build`;
  `ctest --test-dir build --output-on-failure`.
  This includes the VG link smoke and the build-dir-only GAMP/XG/GTF projection smoke.
- Workspace verification passed: `./scripts/verify-workspace.sh`.

## Phase 2 verification

- Low-cost explorer agents completed the CMake/CTest fixture-wiring check and the
  alevin-fry output-file/path check before implementation integration.
- Oracle review found and the implementation resolved blockers in RAD/companion verifier
  coverage, exact projection evidence, source-vs-canonical gene handling, deferred strand
  modes, decision/progress bookkeeping, and single-thread reproducibility.
- Pure build/test passed:
  `cmake --build build-pure`;
  `ctest --test-dir build-pure -L pure --output-on-failure`.
- Full local VG/alevin-fry test suite passed:
  `cmake --build build`;
  `ctest --test-dir build --output-on-failure`.
  The full suite currently contains 18 tests, including the Phase 2 rejected-strand
  checks, primary and repeat `panCollapse convert` runs, byte-identical comparison of
  `map.rad`, `tx2gene.tsv`, and `summary.tsv`, and alevin-fry
  `generate-permit-list`, `collate`, and `quant`.
- Direct RAD/quant evidence is covered by `phase2_quant_verify.py`: `cblen=16`,
  `ulen=12`, raw CB `AAACCCAAGTTTGGGA`, raw UMI `AAAAAAAAAAAA`, one target `TX_A`,
  forward direction, `tx2gene.tsv` equal to `TX_A<TAB>GENE_A`, and a final
  `GENE_A=1` matrix for one cell.

## Gate checklist snapshot

- Gate Architecture Approved: passed in Phase 0.
- Gate Behavior Specified: passed in Phase 1.
- Gate Vertical Slice Proven: passed locally by the current Phase 2 implementation,
  final oracle review, and committed test evidence.
- Phase 2 exact happy path: implemented and verified for one name-grouped GAMP record,
  one source transcript, one canonical target, raw read-name CB/UMI extraction, RAD
  emission, and alevin-fry quantification.
- Phase 2 single-thread reproducibility: represented by repeat conversion and
  byte-for-byte comparison of `map.rad`, `tx2gene.tsv`, and `summary.tsv`.
- Production source is present only for the approved vertical slice. Checked-in generated
  graph/RAD artifacts remain absent.
- Phase 3 strand work: `antisense` and `both` remain V1 requirements, but Phase 2 emits
  to-be-implemented errors for them under D041. Phase 3 must research the best
  implementation strategy before development.

## Session log

- 2026-06-27: Contract and tracked agent workspace created from the workplace-engineering
  principles. No implementation attempted.
- 2026-06-28: All seven pending product questions resolved with option A and incorporated
  into the product, interface, compatibility, and validation contracts.
- 2026-06-28: Phase 0 delegated research completed and integrated. Oracle review found
  blockers in evidence citations, source identity, strand/splice semantics, RAD wire
  details, tag precedence, and progress state; docs were revised to resolve them.
- 2026-06-28: Phase 1 behavior contract completed with focused fixture documents, RAD
  interoperability fixture contract, CMake/CTest skeleton, and passing pure/VG smoke
  tests. Stopped at Gate Behavior Specified for human review.
- 2026-06-28: Phase 2 planning started. User approved raw read-name CB/UMI extraction,
  build-dir-only fixtures, single-thread Phase 2 execution, no Phase 2 USA output, and
  active-doc cleanup away from corrected-tag barcode sourcing.
- 2026-06-29: User clarified that raw CB and UMI lengths are CLI-controlled values and
  approved `--raw-cb-length 16`, `--raw-umi-length 12`, and fixture read
  `read000_AAACCCAAGTTTGGGA_AAAAAAAAAAAA`. The user also asked for RAD `refs`/`dirs`
  semantics to be included in the contracts.
- 2026-06-29: Phase 2 implementation completed for the approved vertical slice. The
  build-dir-only fixture creates GFA/GTF/manifest/JSON GAMP inputs, local `vg` produces
  `.xg` and binary GAMP, `panCollapse convert` emits `map.rad`, `tx2gene.tsv`, and
  `summary.tsv`, a repeat single-thread run is byte-identical, and alevin-fry produces
  the expected 1-cell x 1-gene matrix. User approved deferring `antisense` and `both`
  strand modes to Phase 3 with to-be-implemented errors in Phase 2.
