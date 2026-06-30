# Phase 1 Agent Routing

Phase 1 defines the executable behavioral contract required for **Gate Behavior
Specified**. It does not implement panCollapse behavior. Use this index to load only the
documents needed for a bounded task.

## Always load

- `AGENTS.md`
- `PROGRESS.md`
- `.agent-workspace/GATES.md`
- `docs/product-spec.md`
- `docs/input-output-contract.md`
- `docs/decisions.md`

## Task-specific bundles

| Work area | Load these documents |
|---|---|
| CLI, options, exits, and diagnostics surface | `docs/input-output-contract.md`, `docs/architecture-proposal.md`, `docs/phase1/cli-run-contract.md` |
| VG traversal, projection, and biological compatibility | `docs/compatibility-semantics.md`, `docs/research/annotation-lookup-performance.md`, `docs/phase1/compatibility-fixtures.md`, `docs/phase1/cli-run-contract.md` |
| Score, collapse, assignment, target ordering | `docs/compatibility-semantics.md`, `docs/phase1/policy-fixtures.md`, `docs/phase1/cli-run-contract.md` |
| Barcode/UMI, grouping, manifest, diagnostics | `docs/validation-contract.md`, `docs/phase1/input-diagnostics-fixtures.md`, `docs/phase1/cli-run-contract.md` |
| RAD writer and alevin-fry interoperability | `docs/research/rad-format.md`, `docs/phase1/rad-interop-fixture.md` |
| Build and test integration | `docs/architecture-proposal.md`, `docs/research/vg-integration.md`, `docs/phase1/build-test-plan.md` |
| Gate review or oracle review | All files under `docs/phase1/`, plus `.agent-workspace/GATES.md` |

## Shared Phase 1 rules

- V1 coordinate input remains name-grouped GAMP plus the matching `.xg`.
- `.gcsa`, `.gcsa.lcp`, and `.dist` are upstream `vg mpmap` artifacts, not converter
  inputs.
- GBZ/GBWT are future-only alternatives unless a later approved decision proves equivalent
  path-position behavior.
- Barcode/UMI source text in Phase 1 tag fixtures and CLI snapshots is superseded by
  D038: V1 reads observed raw CB/UMI from the GAMP name field, not corrected/raw GAMP
  annotation tags.
- Fixture expectations must be settled before production behavior code is written.
- Pure policy tests must not include VG headers or link VG libraries.
- VG-dependent tests must be labeled separately from pure tests.
- Summary diagnostics use stable counter names in the fixture contracts; implementation
  may add more counters but must not make the specified counters ambiguous.
- D042 removes the active V1 `--strand` surface. panCollapse preserves RAD `dirs` from
  target-relative orientation; `--max-traversals-per-read` defaults to `100000` with hard
  failure on overflow.
