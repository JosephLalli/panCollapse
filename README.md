# panCollapse

`panCollapse` is a planned C++ tool for converting name-grouped VG multipath alignments
(GAMP) into mapper-style, uncollated RAD records suitable for the standard alevin-fry
single-cell quantification workflow.

The repository currently contains the product specification, behavioral contracts,
decision log, research briefs, staged implementation plan, and Phase 1 CMake/CTest smoke
skeleton. Production implementation has not begun.

## Product intent

The V1 product will:

- consume GAMP records from 10x 3′ single-cell or single-nucleus experiments;
- retain transcript-level compatibility sets, including compatibility inferred from
  annotated introns;
- collapse graph/haplotype/copy-specific transcript paths through an explicit manifest;
- preserve multimapping equivalence classes by default;
- emit uncollated RAD for `alevin-fry generate-permit-list`, `collate`, and `quant`;
- use only existing VG/index and GTF inputs in V1, without introducing a custom lookup
  index.

The implementation language is C++20. The intended compiler environment is GCC 15, and
the build system is CMake with Ninja. The current VG linkage boundary is documented in
`docs/architecture-proposal.md` and smoke-tested in the Phase 1 CTest skeleton. The
project is licensed under Apache License 2.0.

## Project documents

1. [`docs/product-spec.md`](docs/product-spec.md) — canonical V1 contract.
2. [`docs/compatibility-semantics.md`](docs/compatibility-semantics.md) — transcript
   assignment semantics.
3. [`docs/input-output-contract.md`](docs/input-output-contract.md) — external interface
   obligations.
4. [`docs/decisions.md`](docs/decisions.md) — settled decisions and open forks.
5. [`docs/validation-contract.md`](docs/validation-contract.md) — acceptance criteria.
6. [`docs/phase1/README.md`](docs/phase1/README.md) — Phase 1 focused contract routing.
7. [`docs/roadmap.md`](docs/roadmap.md) — explicitly deferred versions and optimizations.

## Current status

Phase 1 executable behavioral contracts are complete and the repository is stopped at
**Gate Behavior Specified** for human review. Do not begin Phase 2 production source,
fixture generation, RAD generation, or vertical-slice implementation until that gate is
approved.
