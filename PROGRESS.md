# Progress

## Current phase

**Phase 3 medium artificial-GAMP RAD semantic fixture is locally implemented and
verified.**

Production source now exists for the approved Phase 2 vertical slice and the selected
Phase 3 orientation, molecule-identity, traversal/scoring, and intron/splice
compatibility behaviors. The user-approved all-compatible-target RAD assignment surface
is implemented locally: `all` is accepted/default, and deferred uniqueness modes fail as
to-be-implemented. The manifest/collapse validation slice now has local test coverage for
max-score source collapse, no score summing across collapsed sources, missing manifest
rows, contradictory rows, and canonical-gene conflicts. No public panCollapse headers or
checked-in generated fixtures, GAMP, XG, GCSA, distance-index, GBZ, or RAD outputs have
been created; generated graph/RAD artifacts remain confined to ignored build directories.
The existing medium fixture uses artificial GAMP generated from reviewed JSON; it is a
50,000-read-group toy-graph RAD semantic regression that remains valid. Per D047 the
production-applicability fixture is now the human-pangenome GAMP-to-RAD plan in
`docs/testing_fixture_creation.md` (pinned MHC `sampleA` spliced bundle, GAMP-driven
independent oracle), which supersedes the earlier BEERS2-plus-`vg mpmap` "primary path"
framing.

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
- panCollapse does not filter compatibility by library strand. It preserves target-
  relative orientation in RAD `dirs`, and downstream alevin-fry expected-orientation
  settings handle library-orientation filtering.
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
  are skipped and counted by default; `--molecule-identity-failures fail` converts those
  conditions into hard failures.
- Collapse-manifest coverage is mandatory and explicit.
- GAMP-to-RAD output uses all-compatible-target assignment. `all` is the default and only
  active RAD assignment behavior; `unique-transcript`, `unique-gene`, and
  `starsolo-default` are deferred to-be-implemented modes outside active RAD conversion.
- The active converter remains single-threaded for now. D045 defers panCollapse-side
  multithreading and redirects future interface research toward streaming GAMP from
  `vg mpmap` into panCollapse over stdin.
- RAD output should be written to disk with a streaming writer: `num_chunks = 0`, file
  tag values, then complete chunks emitted incrementally.
- License: Apache-2.0.

## Next action

Two independent threads are open. (1) Human review of the RAD chunk-count policy conflict
(D045 streaming `num_chunks = 0` versus local alevin-fry v0.15.0 rejecting populated
`num_chunks = 0`); the writer stays exact-count until reconciled. (2) Under D047, build the
human-pangenome GAMP-to-RAD fixture in `docs/testing_fixture_creation.md`, starting with
the smoke slice (a handful of MHC read groups through GAMP -> independent oracle ->
panCollapse -> exact comparison) before scaling.

## Required stop

Do not broaden production code into multithreading without a later explicit approval.
Treat stdin GAMP streaming as research/design work until its input interface is verified.
Do not add stdout RAD output unless separately approved. Phase 3 work must proceed one
independently testable behavior at a time.

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
- `docs/testing_fixture_creation.md` — medium-scale known-truth RAD fixture plan for about
  50,000 BEERS2-generated reads from a pangenome fixture, with independent expected RAD
  records for panCollapse output comparison.

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
  1-gene alevin-fry assertion. Its historical Phase 2 `--strand sense` scope is now
  superseded by D042 for active V1 work.
- RAD docs now explicitly state that production writing is native C++, while libradicl
  and alevin-fry are validation oracles. They also document per-record `bc`, `umi`,
  `refs`, and `dirs` semantics.

## Phase 2 implementation artifacts

- `src/main.cpp` and `src/CMakeLists.txt` — minimal VG-backed `panCollapse convert`
  executable for the approved one-read vertical slice.
- `tests/vg/phase2_fixture_writer.cpp` — build-dir-only GFA, GTF, collapse manifest,
  forward/reverse/mixed-orientation JSON GAMP fixture generator, plus generated-source
  RPVG multipath, connection-score, and vg tiny splice/intron fixtures.
- `tests/vg/phase2_quant_verify.py` — strict Phase 2 verifier for `tx2gene.tsv`, RAD
  header/file tags, packed raw CB/UMI, `refs=[TX_A_ID]`, configurable forward/reverse
  `dirs`, and the final 1-cell x 1-gene alevin-fry matrix.
- `tests/vg/phase3_mixed_orientation_verify.py` — checks that mixed target-relative
  orientation evidence is dropped and counted, and that the resulting diagnostic fixture
  writes a header-only RAD with no chunks.
- `tests/vg/CMakeLists.txt` — CTest chain for fixture generation, local `vg` conversion
  to `.xg` and binary GAMP, primary and repeat single-thread `panCollapse convert` runs,
  byte-for-byte output comparison, forward and reverse alevin-fry permit-list/collate/
  quant flows, mixed-orientation drop verification, and strict output verification.

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
- Oracle re-review passed with no findings, and the verified slice was committed as
  `7396cca Implement Phase 3 orientation slice`.

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
  The historical Phase 2 suite covered primary and repeat `panCollapse convert` runs,
  byte-identical comparison of `map.rad`, `tx2gene.tsv`, and `summary.tsv`, and alevin-
  fry `generate-permit-list`, `collate`, and `quant`.
- Direct RAD/quant evidence is covered by `phase2_quant_verify.py`: `cblen=16`,
  `ulen=12`, raw CB `AAACCCAAGTTTGGGA`, raw UMI `AAAAAAAAAAAA`, one target `TX_A`,
  forward direction, `tx2gene.tsv` equal to `TX_A<TAB>GENE_A`, and a final
  `GENE_A=1` matrix for one cell.

## Phase 3 orientation verification

- VG orientation semantics were checked against the local VG/handlegraph sources by an
  explorer agent before implementation and rechecked after oracle review. Projection
  computes the node-forward span with `node_length - offset - from_length` for reverse
  GAMP mappings, mirrors coordinates when the source path step itself is reverse, and
  records target-relative RAD `dirs`.
- Current full local VG/alevin-fry suite passed:
  `cmake --build build`;
  `ctest --test-dir build --output-on-failure`.
  The suite currently contains 55 tests, including forward and reverse orientation RAD
  verification, reverse `alevin-fry generate-permit-list -d rc`, adjacent-read grouped
  mixed-orientation drop/count behavior, recurrent completed-name failure, two successful
  adjacent read groups, and a two-target record with `TX_A` forward and `TX_B` reverse
  accepted by alevin-fry with `-d either`, exact multi-target quant verification, and a
  no-compatible-read summary/header check. It also covers source paths that traverse a
  node in reverse orientation, original read names containing underscores,
  `N`-containing raw CB/UMI values, default `--molecule-identity-failures skip` for
  missing, malformed, and unsupported raw CB/UMI values, and
  `--molecule-identity-failures fail` as a strict hard failure with `counter=1` in the
  diagnostic for each failure class.
- Pure build/test passed:
  `cmake --build build-pure`;
  `ctest --test-dir build-pure -L pure --output-on-failure`.
- Workspace verification passed: `./scripts/verify-workspace.sh`.

## Phase 3 traversal and compatibility verification

- Low-cost worker agents inspected and materialized scratch conversions from
  `jonassibbesen/rpvg/src/tests/alignment_path_finder_test.cpp` and the local VG
  `test/tiny` fixtures. The generated `.vg`, `.xg`, and `.gamp` scratch artifacts were
  not kept in the repo because D4/D036 require generated graph/alignment/RAD artifacts to
  remain build-dir-only.
- The implementation now enumerates complete `MultipathAlignment` DAG traversals through
  both ordinary `next` edges and scored `connection` arcs, applies
  `--max-traversals-per-read`, sums subpath plus connection scores, projects every
  reference-consuming edit, keeps tied-best targets by default, and applies
  `--score-window`.
- Compatibility now includes exon and implied-intron anchors, exon/intron-boundary
  evidence, annotated splice-junction checks for `connection` edges, and long same-path
  gap checks controlled by `--min-splice-jump`.
- The full local VG/alevin-fry suite passed:
  `ctest --test-dir build --output-on-failure`.
  The suite currently contains 95 tests. New Phase 3 coverage includes RPVG-derived
  multipath traversal over a six-node branching graph, RPVG traversal-cap failure,
  connection-score aggregation with score-window removal and score-window retention,
  same-path short and long gap handling with a `--min-splice-jump` override,
  connection-through-insertion splice preservation, anchored outside-transcript overhang,
  parent-gene-only negative evidence, and vg tiny exon/intron/boundary/splice-junction
  compatibility. It also covers strict numeric CLI parsing for the new traversal/scoring
  options and the existing raw CB/UMI length options.
- Pure build/test passed:
  `ctest --test-dir build-pure -L pure --output-on-failure`.
- High-cost oracle review initially found permissive numeric CLI parsing for the new
  Phase 3 options and existing raw CB/UMI length options. The parser now rejects signed
  values, trailing junk, empty values, and overflow; the added CLI failure tests cover the
  reported cases. Oracle re-review passed with no blocking or non-blocking findings.

## Phase 3 assignment-surface verification

- D044 supersedes the historical uniqueness-policy plan for active GAMP-to-RAD behavior:
  RAD output preserves all compatible retained targets. `--assignment all` is accepted
  and equivalent to the default; `unique-transcript`, `unique-gene`, and
  `starsolo-default` fail with a to-be-implemented diagnostic.
- The full local VG/alevin-fry suite passed:
  `ctest --test-dir build --output-on-failure`.
  The suite currently contains 101 tests, including explicit `--assignment all` coverage
  on the multi-target RAD/alevin-fry fixture, a default-vs-explicit-`all` byte comparison,
  unknown-assignment usage-error coverage, and deferred-mode failure tests for all three
  inactive uniqueness modes.
- The first high-cost oracle review found stale removal-counter/uniqueness wording and
  missing assignment-surface evidence. The stale wording was corrected, the missing CTest
  coverage was added, and high-cost oracle re-review passed with no blocking findings.
- Pure build/test passed:
  `ctest --test-dir build-pure -L pure --output-on-failure`.
  Workspace verification passed: `./scripts/verify-workspace.sh`.

## Phase 3 manifest/collapse verification

- The implementation already enforced the manifest source key and mapped compatible source
  identities to canonical RAD targets. The new validation slice adds direct fixtures for
  Section 5 manifest/collapse requirements.
- The local manifest-focused suite passed:
  `ctest --test-dir build -L manifest --output-on-failure`.
  It covers two source transcript identities collapsing to one canonical target using the
  maximum source score, graph/source multiplicity not inflating a collapsed target score by
  summing, missing source identity hard failure, contradictory manifest row hard failure,
  and canonical transcript mapped to multiple genes hard failure.
- The full local VG/alevin-fry suite passed:
  `ctest --test-dir build --output-on-failure`.
  The suite currently contains 110 tests.
- High-cost oracle review initially found that the max-score fixture could pass a
  last-compatible-source-wins bug. The fixture now covers both low-to-high and high-to-low
  collapsed-source score order, and oracle re-review passed with no blocking findings.

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
- Production source has been broadened from the Phase 2 vertical slice for Phase 3:
  multi-row manifests, multiple GTF transcript models, grouped GAMP records,
  multi-target RAD records, target-relative `dirs`, complete GAMP traversal enumeration,
  score-window filtering, and exon/intron/splice compatibility are implemented.
  Checked-in generated graph/RAD artifacts remain absent.
- Manifest/collapse validation now directly verifies max-score collapse, no score summing
  across collapsed source identities, missing source identity failure, contradictory row
  failure, and canonical-gene conflict failure.
- D044 supersedes the old assignment-policy plan for active GAMP-to-RAD behavior:
  preserve all compatible retained targets in RAD, accept/default to `all`, and treat
  `unique-transcript`, `unique-gene`, and `starsolo-default` as to-be-implemented future
  modes rather than RAD filters.
- Phase 3 orientation work supersedes the old strand-mode plan under D042: remove
  `--strand`, preserve target-relative RAD `dirs`, and drop/count mixed-orientation
  evidence for one emitted target.
- Medium artificial-GAMP RAD semantic coverage now generates 50,000 read groups (77,500
  input records) across a four-node/four-path graph, FASTQ, GFA/XG, GTF, collapse
  manifest, JSON GAMP converted to binary GAMP, expected semantic RAD rows, expected
  non-emitted rows, and panCollapse RAD. Its scaled case mix now exercises multi-gene and
  multi-isoform overlap, unique single-target emission, two source paths collapsing to one
  canonical target `TX_H` with an equal-score companion `TX_B` that proves no score
  summing, collapse to a single canonical target, top-score retention with score-window
  removal, target-relative forward/reverse `dirs`, mixed-orientation drops, no-compatible
  drops, unaligned read groups, and a malformed raw molecule-identity skip. It verifies
  exact target sets and per-target orientation, the alphabetical RAD target dictionary,
  every Section 12 counter, `tx2gene.tsv`, and RAD record semantics, and a coarse
  `TIMEOUT` guards the convert against catastrophic throughput regressions. It remains an
  artificial-GAMP regression and does not exercise the FASTQ -> `vg mpmap` -> GAMP mapper
  path.
- D043 adds the active raw molecule-identity failure policy:
  `--molecule-identity-failures skip|fail`, default `skip`, with
  `raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
  `raw_molecule_unsupported_groups`, and `raw_molecule_skipped_groups`.
- D046 adds the `unaligned_reads` counter: read groups whose GAMP records all have zero
  subpaths emit no RAD record and increment `unaligned_reads` additively alongside
  `no_compatible_transcript_groups`, without being treated as molecule-identity failures.
- The all-dropped mixed-orientation diagnostic fixture is not an alevin-fry
  interoperability fixture. Local alevin-fry rejects no-chunk RAD files, so nonempty
  forward and reverse orientation fixtures carry the current alevin-fry proof.
- A local implementation experiment confirmed the same conflict for populated RAD:
  libradicl can represent unknown chunk count as `num_chunks = 0`, but local alevin-fry
  v0.15.0 `generate-permit-list` rejects populated `num_chunks = 0` input as no chunks.
  The current production writer remains exact-count/backpatched-style until the user
  approves how to reconcile D045 with the supported alevin-fry path.

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
- 2026-06-29: User superseded the panCollapse strand-mode plan with target-relative RAD
  orientation preservation: remove `--strand`, do not filter by strand, write real
  forward/reverse `dirs`, and drop/count mixed orientations for one emitted target.
- 2026-06-29: User approved the raw molecule-identity failure surface
  `--molecule-identity-failures skip|fail`, default `skip`, with stable
  `raw_molecule_*` counters. The implementation now skips/counts missing, malformed,
  and unsupported raw CB/UMI groups by default and fails strictly under `fail`.
- 2026-06-30: Phase 3 orientation and raw molecule-identity slice was oracle-reviewed,
  verified locally, and committed as `7396cca`.
- 2026-06-30: Phase 3 traversal/scoring plus intron/splice compatibility was implemented
  with build-dir generated RPVG-derived and vg tiny fixtures. Full local VG/alevin-fry
  tests passed with 95 tests after strict numeric CLI parser coverage was added, and
  pure tests passed. High-cost oracle re-review passed.
- 2026-06-30: User superseded historical assignment-policy scope for active GAMP-to-RAD
  behavior: RAD output preserves all compatible retained targets; uniqueness modes are
  deferred outside active RAD conversion. Local full suite passed with 101 tests after
  addressing first-pass oracle findings; high-cost oracle re-review passed.
- 2026-06-30: Added direct Phase 3 manifest/collapse validation fixtures. Local manifest
  label passed and full local VG/alevin-fry suite passed with 110 tests after addressing
  first-pass oracle findings; high-cost oracle re-review passed.
- 2026-06-30: Added `docs/testing_fixture_creation.md` to the local documentation index as
  the medium-scale known-truth RAD fixture plan: about 50,000 BEERS2 reads from a
  pangenome fixture with independently computed expected RAD records.
- 2026-07-01: User deferred panCollapse-side multithreading for now and identified direct
  stdin GAMP streaming from `vg mpmap` into panCollapse as a future interface direction to
  research. User also approved streaming RAD-to-disk output with `num_chunks = 0` and
  complete chunks emitted incrementally.
- 2026-07-01: Added the medium artificial-GAMP RAD semantic fixture. The local full suite
  passed with `ctest --test-dir build --output-on-failure -j 8` at 115/115 tests.
  `git diff --check` and `./scripts/verify-workspace.sh` also passed before commit.
  High-cost oracle review returned PASS WITH RISKS: no blocking findings, with the
  remaining BEERS2 plus `vg mpmap` path and RAD chunk-count policy conflict called out
  for human review.
- 2026-07-01: Landed the unaligned read-group diagnostic (D046) that a prior interrupted
  session left uncommitted. A GAMP read group whose records all have zero subpaths emits
  no RAD record and increments the additive `unaligned_reads` counter alongside
  `no_compatible_transcript_groups`. Added a name-only GAMP fixture, the
  `phase3_unaligned_*` builder/convert/verify CTest chain, and the `verify_unaligned`
  check. The local full suite passed at 118/118 tests and the pure suite passed.
- 2026-07-01: Reviewed the artificial-GAMP medium fixture for throughput, multimapping,
  and required-representation coverage, then extended it (no aligner) per user direction.
  The scaled mix now adds many-source->one-canonical collapse with a no-score-summing
  companion, collapse-to-single-target, unique single-target emission, unaligned groups,
  and a malformed molecule-identity skip, growing the graph to four nodes/paths and
  77,500 input records. A first oracle version mispredicted the malformed case as
  unsupported (`N` is a supported molecule base, `main.cpp:410`); corrected to a
  wrong-length UMI. Added a coarse `TIMEOUT` throughput guard on the convert. Full local
  suite passed at 118/118 and pure passed. The fixture remains artificial GAMP and does
  not prove the FASTQ -> `vg mpmap` -> GAMP mapper interface, which still needs a real-
  mpmap fixture (direct template slicing recommended over full BEERS2 for the strict
  oracle).
- 2026-07-01: User rescoped panCollapse as a GAMP -> RAD bridge (D047): FASTQ -> `vg mpmap`
  is out of scope, the production-applicability fixture must come from the human pangenome
  (pinned MHC `sampleA` spliced bundle), and the RAD oracle is GAMP-driven and independent
  of panCollapse output. Rewrote `docs/testing_fixture_creation.md` from the ~1500-line
  BEERS2-plus-`vg mpmap` plan to a 208-line human-pangenome GAMP-to-RAD plan, retiring the
  "artificial GAMP must never be primary" and mapping-stability rules. Verified all eight
  pinned MHC source files exist with sizes matching the recorded `size_bytes`. Docs-only
  change; no production code touched. Not yet committed.
