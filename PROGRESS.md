# Progress

## Current phase

**Phase 3, redesigned around D048 (graph-native transcript feature counting).**

The GAMP-to-RAD core is now specified in `docs/conversion-algorithm.md`: score each aligned
node under vg's own scoring scheme, add each node's score to every HST path crossing it, take
the top HST score across all of a read's alignments plus ties, collapse to unique transcript
IDs for RAD `refs`, and record orientation from the read's direction along the HST path
(majority of aligned bases on disagreement). Runtime inputs are the name-grouped GAMP, the
graph with `vg rna` HST paths, and a transcript-to-gene map; the GTF and `vg rna` are
reference/fixture-creation only.

The prior traversal-enumeration and GTF-projection implementation in `src/main.cpp`
(D026/D027) is superseded by D048 and is being replaced increment by increment; it removes
the complete-traversal enumeration whose cap hard-failed on real MHC GAMP. RAD wire format,
streaming writer, raw CB/UMI sourcing, name-grouping, unaligned counting, and the
human-pangenome fixture plan (D047) are unchanged. Generated graph/RAD artifacts remain
confined to ignored build directories.

## Settled product decisions

D048 supersedes the assignment-surface, collapse-manifest, traversal-cap, and GTF-projection
items below for active GAMP-to-RAD conversion; see `docs/conversion-algorithm.md`. The
bullets are retained as cumulative history.

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

All six PathTally increments (`docs/conversion-algorithm.md`) plus D049 streaming (stdin
input via `--gamp -` and a seek-and-backpatch RAD writer) are implemented and verified; the
RAD chunk-count conflict is resolved by D049 with exact backpatched counts that alevin-fry
accepts. The old traversal-era CTest fixtures are retired, a self-contained end-to-end CTest
is in place (committed tiny graph + independent oracle), and the per-node HST lookup now
memoizes each distinct node's crossings (D051): the 1M-read MHC run dropped from ~3m47s to
~2m44s (about 28%), byte-identical, 30/30 CTest. Further
speedups (reintroducing threads under a later approval per D045) remain future work; a
sorted/indexed GAMP was analyzed and judged not worthwhile (D051).

## Required stop

Do not broaden production code into multithreading without a later explicit approval.
Stdin GAMP streaming is implemented (D049, `--gamp -`). Do not add stdout RAD output unless
separately approved (the seek-and-backpatch writer needs a seekable file). Phase 3 work must
proceed one
independently testable behavior at a time, following D048 and
`docs/conversion-algorithm.md`. Do not reintroduce traversal enumeration, runtime GTF
projection, or a custom index.

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
  change; no production code touched. Committed as `281aae1`.
- 2026-07-01: Session-start verification for the D047 human-pangenome smoke slice.
  Re-verified all eight pinned MHC source files by sha256 (all `OK`) and confirmed the local
  `vg` is the pinned `v1.75.0-68-ge82694b69 "Spike"`. Characterized the bundle from primary
  sources: `sampleA.spliced.xg` has 353,766 nodes (id range 1:353766), 488,650 edges, and
  27,013 paths, of which 27,011 are HST transcript-haplotype paths (`<transcript>_H<n>` /
  `_R<n>`) and 2 are genomic backbones (`GRCh38#0#chr6[28510118]`, `CHM13#0#chr6[28381547]`).
  `mhc.refpath.gtf` annotates a single seqname `GRCh38#0#chr6[28510118]` (1,022 transcripts,
  11,033 exons) that exactly matches a visible `.xg` path, satisfying the D023 seqname/path
  match on the reference path. Surfaced the smoke-slice source-path/collapse-model fork now
  recorded under Next action. No production code, fixtures, or generated artifacts created.
- 2026-07-01: Ran scoped read-only research (librarian + vg-integration-researcher +
  Explore) on building a production-representative human-pangenome GAMP-to-RAD fixture from
  the full HPRC pangenome, and verified the load-bearing claims locally. Findings: (a) the
  full HPRC v1.1 MC GRCh38 pangenome is already on disk at
  `/mnt/ssd/lalli/nf_stage/genome_refs/pangenome/GRCh38/` (`hprc-v1.1-mc-grch38.d9.gbz`
  4.4G, `.full.gbz` 7.1G, `.xg`, `.dist`), HPRC S3 provenance recorded in `to_download.txt`;
  no download needed. (b) The pinned `vg` provides the whole recipe (confirmed flags): `vg
  chunk` (`-p`/`--gbz`/`--contig`/`-c`/`-T`/`-G`), `vg rna` (`-z`/`-l`/`-e`/`-r`/`-a`/`-i`),
  `vg autoindex --workflow mpmap`, `vg sim` (`-P`/`-a`/`--multi-position`, truth in GAM
  refpos not read name), `vg mpmap -F GAMP` (default), and `vg gampcompare`. (c) The
  existing MHC bundle was NOT sliced from HPRC; its source is a toy 466-haplotype
  `1kg_hg38-MHC.gbz` (panresolve example, unrecorded upstream URL) built with native vg
  1.51.0, so a fresh HPRC-derived pinned-vg build is a provenance and version upgrade. (d)
  Prior scratch in `build/real_mhc_scratch/` (today) already tagged real 10x reads
  (`SRR10587809` subsample), produced a 228 MB real `mhcA.gamp` against the toy bundle,
  built a reference-path collapse manifest (`mhc_identity_collapse.tsv`, exact D033 format,
  `source_path_name=GRCh38#0#chr6[28510118]`, source==canonical, 1022 transcripts), and ran
  panCollapse -> HARD FAIL `traversal_cap_exceeded_groups=1`: real MHC GAMP exceeds the
  default `--max-traversals-per-read 100000` on at least one read group. Two build-blocking
  points recorded: a `vg chunk -p` region slice of a GBZ drops the GBWT haplotypes `vg rna`
  needs (use `-T`/embedded paths or `--gbz --contig`), and the D037 traversal-cap hard-fail
  policy needs a production-graph decision. Research only; no production code, fixtures, or
  committed artifacts created.
- 2026-07-01: Adopted D048 and rewrote the planning docs around it. The GAMP-to-RAD core is
  now graph-native transcript feature counting: score aligned nodes under vg's own scheme,
  attribute to the HST paths crossing each node, take the top HST score across all of a
  read's alignments plus ties, collapse HST copies to unique transcript IDs, and record
  orientation by majority of aligned bases. This removes complete-traversal enumeration (and
  the cap that hard-failed on real MHC GAMP), runtime GTF projection and exon/intron/junction
  compatibility, and the explicit collapse manifest, and it dissolves the earlier
  reference-path-vs-HST fork. Confirmed from vg source that per-node scoring reproduces
  `Subpath.score` (`alignment_scorer.cpp`) and that HST paths are stored transcript 5'->3'
  (`transcriptome.cpp`). Wrote `docs/conversion-algorithm.md`, added D048, rewrote
  `docs/compatibility-semantics.md`, and updated `docs/product-spec.md`, `TASKS.md`,
  `PROGRESS.md`, and the `CLAUDE.md` index and principles. Remaining doc-ecosystem updates
  (input/output contract, validation contract, fixture oracle, glossary, historical
  pointers) in progress. No production code changed yet.
- 2026-07-02: PathTally increment 1 implemented and verified. Added `src/pathtally_score.hpp`
  (per-node reproduction of vg's `score_partial_alignment`: match +1, mismatch -4, affine
  gaps 6/1, +5 read-end bonus, soft-clip and affine-carry handling, plus a subpath read-offset
  propagator) and `tests/vg/pathtally_score_test.cpp`. Synthetic unit asserts hold exactly;
  the committed CTest (`pathtally_score_test`) runs those. Against the real MHC GAMP
  (`build/real_mhc_scratch/mhcA.gamp`, 13.1M subpaths) the flat scorer reproduces
  `Subpath.score` exactly for 93.14%; the remainder are base-quality-adjusted lower-quality
  bases (confirmed: deviating subpaths mean base quality 22.4 vs 34.4 for exact matches, and
  single-base mismatches score -3 at q25 / -2 at q11 instead of -4). Adopted the flat per-node
  score as the ranking weight; exact quality-adjusted reproduction deferred as unnecessary.
- 2026-07-02: PathTally increments 2-5 implemented and verified, plus the optional
  quality-adjusted scorer. Added `src/pathtally.hpp` (node-to-HST attribution via injected
  lookup, transcript-id collapse, top-score-plus-ties winner selection across a read's
  alignments, majority-of-bases orientation) with `tests/vg/pathtally_pipeline_test.cpp`
  (synthetic, all cases PASS). Rewrote `src/main.cpp` on PathTally: inputs are now
  `--gamp/--xg/--t2g` (no GTF/manifest), with `--score flat|qualadj`; it converts the real
  1M-read MHC GAMP (against `hst_index`) to a valid `map.rad` (39,103 records),
  `tx2gene.tsv` (1022 transcripts), and summary in ~6 min (per-node path lookup is the perf
  hotspot). Reimplemented vg's QualAdjAligner in `src/pathtally_qualadj.hpp` (recovered
  log-base, quality-indexed log-odds matrix, per-base scoring) and validated it reproduces
  `Subpath.score` for 99.2% of subpaths (residual +/-1 is score-matrix rounding). alevin-fry
  0.15.0 consumed the real PathTally RAD through generate-permit-list (46 corrected barcodes),
  collate (257 chunks), and quant to a 257-cell x 317-gene count matrix, completing increment
  5's interop on real data. Increment 6 is also verified: an independent Python oracle
  (`tests/vg/pathtally_oracle.py`, reimplementing steps 1-4 from the graph P-lines without
  using PathTally code) matched panCollapse's RAD exactly for all 420 emitted records across a
  10,551-read-group real MHC-GAMP sample. All six PathTally increments are implemented and
  verified. Remaining (hygiene, not algorithm): retiring the old traversal-era CTest fixtures
  (`phase2_*`/`phase3_*`/projection, which invoke the removed CLI), self-contained CTests for
  the real-data score and oracle checks (they depend on build-dir scratch), and the
  per-node-lookup performance work. New code (`src/pathtally*.hpp`, rewritten `src/main.cpp`,
  `tests/vg/pathtally_*`) is uncommitted.
- 2026-07-02: Verified PathTally on a deterministic `vg sim` fixture. `vg sim -x spliced.xg
  -P <four HST paths> -n 400 -l 90 -s 1 -q` -> `tests/vg/fixture_tag_reads.py` (synthetic
  CB/UMI) -> `vg mpmap -n rna -F GAMP` -> panCollapse emitted RAD for all 400 read groups,
  byte-identical on re-run, with the four source transcripts in the dictionary; the
  independent oracle matched all 400 records exactly. Committed the PathTally implementation
  as `07000a0` and the vg sim fixture verification as `6bc16eb`.
- 2026-07-02: Implemented streaming GAMP input and a seek-and-backpatch streaming RAD writer
  (D049). `--gamp -` reads GAMP from stdin (`vg mpmap ... | panCollapse convert --gamp -`); the
  writer emits the header, target dictionary, and tags with a placeholder `num_chunks`, streams
  each record as its group flushes, and seeks back at finalize to patch the exact chunk
  byte/record counts and `num_chunks`. Verified byte-identical to the buffered writer on the
  sim fixture, byte-identical between file and stdin input, the oracle still exact, and a
  no-emit run leaves a header-only `num_chunks = 0` file. This supersedes D045's
  `num_chunks = 0` output and resolves the chunk-count-versus-alevin-fry conflict.
- 2026-07-02: Cleaned up the test suite and optimized the lookup. Retired the old traversal-era
  CTest fixtures (`phase2_*`, `phase3_*`, GTF-projection smoke) that invoked the removed CLI,
  and added a self-contained end-to-end CTest: a committed tiny graph + multipath GAMP
  (`tests/vg/fixtures/pathtally_smoke/`) is converted to xg/GAMP by vg, run through panCollapse,
  and checked exactly by the independent oracle (`pathtally_smoke_*`; 3 emitted records
  covering haplotype collapse, a multi-gene record, and reverse orientation). Optimized the
  per-node HST lookup by precomputing a path-handle -> HST-name map (removing per-step
  `get_path_name` + string-set lookup); output stays byte-identical and the 1M-read MHC run
  dropped from ~5m55s to ~3m57s. The pathtally + pure suite passes 9/9.
- 2026-07-04: Cleared the pre-release review (`docs/v0.1-review-findings.md`) for a full v0.1.
  Blockers: multi-chunk RAD writer with per-chunk and file-level seek-and-backpatch removing the
  single-chunk u32 ceiling (B3); hermetic alevin-fry interop CTest asserting the exact
  cell-by-gene matrix (B1); scoring-fidelity CTest that simulates reads from the HST paths, maps
  them with mpmap, and checks the flat and quality-adjusted per-node scorers against vg's own
  Subpath.score, plus stdin byte-identity and molecule/recurrence failure-mode tests (B2). Then
  every non-blocking finding N1-N11: spec reconciliation to D048/D049 (N1); documented
  completed_names memory cost (N2); GAMP/xg node-id-space hard failure with a regression fixture
  (N4); quality-adjusted scorer bounds checks (N5); qualadj cross-platform determinism caveat
  (N6); parameterized and documented build paths (N7); -Wall -Wextra on project targets (N8);
  --version/--help success paths (N9); atomic map.rad via temp-path-and-rename (N10); and the
  per-emitted-group target-size histogram in summary.tsv (N11). N3 was resolved earlier as
  working-as-intended. Recorded as D050. Full CTest suite: 30/30. Commits c4df275 (blockers),
  e7e2156 (N1), then 29b6794, a3a38e9, bf31524, 9fd1e3a, 92bc8a1, a8e5bea, c675cc8, 90b9a21,
  1bcc288 (N-tasks), 1aa2567 (findings closeout).
- 2026-07-05: Performance session. Optimized the per-node HST lookup by memoizing each
  distinct node's HST crossings on first visit (lazy cache in `src/main.cpp`), so a node's
  path steps are scanned at most once per run instead of once per node-visit. Measured on the
  real 1M-read MHC GAMP against `sampleA` `spliced.xg`: wall clock 3:47 -> 2:44 (about 28%,
  1.39x), `map.rad`/`tx2gene.tsv`/`summary.tsv` byte-identical (sha256 `7df66891...` on
  map.rad), peak RSS +~16 MB, full CTest 30/30. Recorded as D051, which also captures two
  requested analyses: the run is ~99% single-core CPU-bound so per-read-group work is the
  natural parallel unit (producer/worker/writer with an order-preserving reorder buffer for
  byte-identical output), identification only under D045; and a sorted/indexed GAMP is judged
  not worthwhile because the tool is a single linear streaming pass with no random access.
  Committed on branch `perf/node-hst-cache` as `f3bda22`.
- 2026-07-05: Found the dominant lever while profiling: the build set no `CMAKE_BUILD_TYPE`,
  so the shipped binary (and the v0.1 Docker image, which packages `build/src/panCollapse`)
  compiled at `-O0`. Building `-O2` (RelWithDebInfo) cut the 1M-read MHC run from 2:44 to
  0:45.6 (about 3.6x, ~5x over the original `-O0`), byte-identical, CTest 30/30; `-O3` was
  within noise (0:46.1) and loses symbols, so RelWithDebInfo is the default. Made the
  optimized build the default in `CMakeLists.txt`, bumped the version to 0.2.0, and built the
  `josephlalli/pancollapse:v0.2` runtime image from the `-O2` binary. Verified the image
  reports `panCollapse 0.2.0` and that a containerized smoke conversion is byte-identical to
  the host `-O2` run. Recorded as D052.
- 2026-07-06: Added an opt-in BAM output (`--bam-out`, D054) for a CellRanger-style counting
  stack (`umi_tools count --per-gene --gene-tag=XT` -> DropletUtils `emptyDropsCellRanger`),
  designed via plan-mode approval since a new output backend returns to a gate. One mapped
  record per emitted read on a synthetic per-gene contig, carrying 10x tags (`CB`/`CR`,
  `UB`/`UR`, full-set `GX`, `GN`, and single-gene-or-omitted `XT`); genes come from the same
  t2g the RAD uses, so no surjection and no new runtime input. New `BamWriter` (htslib, already
  a transitive dep) in `src/main.cpp`; `cmake/PanCollapseVg.cmake` links `htslib`. Ten hermetic
  `bam`-label CTests (quickcheck, header, RAD byte-identity with/without `--bam-out`, oracle
  `GX` parity, `umi_tools count` vs a naive tag recount, position independence); full suite
  40/40. Real 1M-read MHC: `map.rad` byte-identical (sha `7df66891...`), BAM 39,103 records over
  317 gene contigs, and the sort/index + `umi_tools count` pipeline yields 30,105 molecules over
  9,701 cells x 203 genes (2,755 multi-gene reads skipped for lacking `XT`). Bumped to 0.3.0.
  Documented in `docs/bam-export.md`, README, and `docs/input-output-contract.md`. umi_tools
  1.1.6 installed to `~/.local`; the BAM tests need a python with pysam (the pysam+umi_tools
  interpreter is `python@3.11`, passed via `-DPython3_EXECUTABLE`).
- 2026-07-07: Added GeneFull (intron-inclusive) counting (D055) so intronic reads call their gene,
  closing the exon-only gap vs STARsolo `--soloFeatures GeneFull` (surfaced while explaining a
  counter comparison: the STAR-CR arm counted introns via GeneFull; panCollapse is spliced-only,
  an ~6-7% MHC GEX confound). Final design uses **no panCollapse code or flag**: GeneFull is the
  ordinary spliced count run against a graph annotated with gene-body paths, selected by the t2g.
  panCollapse reads gene-body membership off embedded paths exactly as it reads transcript
  membership off HST paths, so the same graph gives a spliced count (HST t2g) or a GeneFull count
  (gene t2g). `scripts/make-gene-annotation.sh` embeds one unspliced gene-body path per gene
  (`vg rna --feature-type gene`, `-l` for alt coverage) and writes the gene t2g. An interim
  attempt (a `--gene-mode full` flag + `--gene-loci` node->gene TSV + parallel lookup) was removed
  before merge: it duplicated the graph's own path index (a custom node->gene map is the very
  "custom index" the transcript path avoids), and spliced mode already did GeneFull with a
  gene-body t2g. Verified edge cases: exon-intron boundary and multi-exon+spanned-intron reads are
  called either way (the exon node is on the HST), so GeneFull's only marginal call is the
  purely-intronic read; overlapping bodies give multi-gene on full overlap and the dominant gene
  on partial overlap; a single t2g must not list both layers. Hermetic `genefull` CTests annotate
  the fixture graph and show the SAME graph drops the intron with the HST t2g and calls it with
  the gene t2g. Full suite 45/45; the converter is byte-identical to v0.3 (no binary change).
  Docs in `docs/genefull.md` + README.
- 2026-07-07: Added an opt-in target-relative orientation filter `--strand {both,forward,reverse}`
  (D056), applied after winner selection: `forward` keeps sense targets, `reverse` antisense,
  `both` (default) filters nothing. Reads whose targets are all filtered emit no record and are
  counted in a new `strand_filtered_groups` summary counter. This reintroduces (as opt-in) the
  strand filtering D042 had pushed downstream, matching STARsolo `GeneFull_Ex50pAS`/`--soloStrand`;
  the default `both` is byte-identical to before and still writes `dirs`. Hermetic `strand` CTests
  on the smoke fixture (readfwd/readmulti forward, readrev reverse): `forward` emits 2
  (`strand_filtered_groups=1`, no readrev), `reverse` emits 1 (`=2`). Full suite 47/47.
- 2026-07-05: Profiled the `-O2` binary (callgrind, 1M-read MHC) and acted on it (D053). The
  scorer is ~1%; the run is dominated by GAMP ingestion (~60%) and the per-group tally (~34%),
  with malloc/free (~27%) and string-compare memcmp (~13%) the top categories. Multithreading
  judged low value (only ~34% parallelizable, Amdahl-capped near 1.5x without BGZF-stream
  sharding). Applied two byte-identical single-threaded wins: move each parsed record into the
  group buffer instead of deep-copying (`main.cpp`), and key the per-group tally with a reused
  `absl::flat_hash_map` workspace instead of a per-group `std::map` (`pathtally.hpp`,
  `main.cpp`, plus `absl_raw_hash_set` in `cmake/PanCollapseVg.cmake`). Measured 45.6s -> 41.2s
  (move) -> 30.4s (flat tally), byte-identical each step, RSS 393 -> 281 MB, CTest 30/30. End to
  end the v0.2 build is ~7.5x faster than the original `-O0` v0.1 (227s -> 30s), identical
  output. Rebuilt the `josephlalli/pancollapse:v0.2` image on the fully-optimized binary.
