# Progress

## Current phase

**Phase 1 executable behavioral contract is complete. Phase 2 planning is in progress.**

No production source, public panCollapse headers, or checked-in generated fixtures,
GAMP, XG, GCSA, distance-index, GBZ, or RAD outputs have been created. The repository
is stopped before Phase 2 implementation.

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
- Raw CB and UMI lengths are CLI-controlled values with defaults matching the fixture
  lengths used by the implementation test.
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

Complete skeptical oracle review of the Phase 2 planning docs, then ask the user directly
for any remaining CLI option spelling or numeric default approvals needed before
implementation.

## Required stop

Do not begin Phase 2 production panCollapse source, fixture generation, RAD generation,
or vertical-slice implementation until the implementation gate, exact fixture raw CB/UMI
values, numeric length defaults, and length-option spellings are approved.

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
  1-gene alevin-fry assertion.
- RAD docs now explicitly state that production writing is native C++, while libradicl
  and alevin-fry are validation oracles. They also document per-record `bc`, `umi`,
  `refs`, and `dirs` semantics.

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

## Gate checklist snapshot

- Every settled behavior has an explicit fixture and expected result: yes, split across
  `docs/phase1/compatibility-fixtures.md`, `policy-fixtures.md`, and
  `input-diagnostics-fixtures.md`.
- Outside-first/last-exon and positive transcript-model anchor represented: yes,
  `CMP-09`.
- Raw read-name CB/UMI parsing, skip/strict/fail behavior, and complete manifest
  coverage represented: in progress for the Phase 2 plan. Older Phase 1 `TAG-*`
  corrected/raw annotation-tag fixtures are historical under D038; manifest coverage
  remains represented by `MAN-01` through `MAN-07`.
- Policy ordering score/collapse/uniqueness is unambiguous: yes, `SC-*`, `COL-*`, and
  `POL-*`.
- CLI snapshot, option defaults, traversal cap, and exit surface are explicit: yes,
  `docs/phase1/cli-run-contract.md`.
- RAD interoperability has exact expected matrix: yes,
  `docs/phase1/rad-interop-fixture.md`.
- Failure and diagnostic behavior is testable for Phase 1 historical contracts; raw
  read-name molecule-identity diagnostics are being recentered under D038 before Phase 2
  implementation.
- Build/test skeleton matches approved architecture: yes, pure/VG tests are separated and
  the VG link smoke passes against the local build family.
- Production panCollapse source, checked-in generated fixtures, and RAD outputs absent:
  yes.
- Human gate approval still required before Phase 2.

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
- 2026-06-29: User clarified that raw CB and UMI lengths are CLI-controlled values with
  defaults matching the fixture lengths used by the implementation test, and asked for
  RAD `refs`/`dirs` semantics to be included in the contracts.
