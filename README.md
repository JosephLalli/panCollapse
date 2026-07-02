# panCollapse

`panCollapse` is a planned C++ tool for converting name-grouped VG multipath alignments
(GAMP) into mapper-style, uncollated RAD records suitable for the standard alevin-fry
single-cell quantification workflow.

The repository contains the product specification, behavioral contracts, decision log,
research briefs, the staged implementation plan, and a CMake/CTest skeleton. An earlier
traversal-based converter exists in `src/`, superseded by decision D048; the D048 algorithm
(`docs/conversion-algorithm.md`) is the current direction and its implementation has not yet
begun.

## Product intent

The V1 product will:

- consume GAMP records from 10x 3′ single-cell or single-nucleus experiments;
- extract observed raw cell barcodes and UMIs from the GAMP name field;
- determine transcript compatibility via HST-path membership: a read is compatible with a
  transcript when one of that transcript's haplotype-specific transcript (HST) paths
  (`<transcript_id>_H<n>` / `_R<n>`, embedded by `vg rna`) traverses a node the read
  aligns to;
- score each aligned node under vg's own alignment scheme and select the winning transcript
  set as the top HST score across all of the read's alignments plus ties;
- collapse winning HST paths to unique transcript IDs implicitly through the HST naming
  convention, with no separate runtime collapse manifest;
- preserve multimapping equivalence classes by default;
- emit uncollated RAD for `alevin-fry generate-permit-list`, `collate`, and `quant`;
- use only the name-grouped GAMP, the matching `.xg` graph carrying HST paths, and a
  transcript-to-gene map at runtime, without a GTF, a collapse manifest, or a custom
  lookup index.

The implementation language is C++20. The intended compiler environment is GCC 15, and
the build system is CMake with Ninja. The current VG linkage boundary is documented in
`docs/architecture-proposal.md` and smoke-tested in the Phase 1 CTest skeleton. The
project is licensed under Apache License 2.0.

## Project documents

1. [`docs/product-spec.md`](docs/product-spec.md) — canonical V1 contract.
2. [`docs/conversion-algorithm.md`](docs/conversion-algorithm.md) — canonical GAMP-to-RAD
   algorithm (D048).
3. [`docs/compatibility-semantics.md`](docs/compatibility-semantics.md) — transcript
   assignment semantics.
4. [`docs/input-output-contract.md`](docs/input-output-contract.md) — external interface
   obligations.
5. [`docs/decisions.md`](docs/decisions.md) — settled decisions and open forks.
6. [`docs/validation-contract.md`](docs/validation-contract.md) — acceptance criteria.
7. [`docs/phase1/README.md`](docs/phase1/README.md) — Phase 1 focused contract routing
   (historical; see D048 banner).
8. [`docs/phase2/implementation-plan.md`](docs/phase2/implementation-plan.md) — Phase 2
   vertical-slice plan (historical; see D048 banner).
9. [`docs/roadmap.md`](docs/roadmap.md) — explicitly deferred versions and optimizations.

## Current status

The GAMP-to-RAD algorithm was redirected by decision D048 to graph-native transcript feature
counting (`docs/conversion-algorithm.md`). The earlier traversal-enumeration and
GTF-projection converter in `src/` is superseded and will be replaced increment by increment.
`PROGRESS.md` is the authoritative state tracker.
