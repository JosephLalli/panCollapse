# panCollapse — Agent Index

Design and implement the GAMP-to-RAD product specified in `docs/product-spec.md`. This is
an index, not the full manual; read detailed files only when working in their area.

## Principles

- Verify external APIs and formats before designing against them.
- Do not write production code until Gate Architecture Approved has passed.
- Preserve multimapping evidence. Per D048, GAMP-to-RAD emits the read's compatible
  transcript set: score aligned nodes under vg's scheme, attribute to the HST paths crossing
  them, take the top HST score across all of a read's alignments plus ties, and collapse to
  unique transcript IDs. See `docs/conversion-algorithm.md`.
- Runtime inputs are the GAMP, the graph with `vg rna` HST paths, and a transcript-to-gene
  map; the GTF and `vg rna` are fixture-creation only. Do not introduce a custom lookup index
  without measured evidence and human approval.
- Update `PROGRESS.md` and `docs/decisions.md` before ending a substantive session.

## Hard rules

@.claude/rules/project.md
@.claude/rules/languages.md
@.claude/rules/scope.md
@.claude/rules/provenance.md

## Common commands

- `/session-start`
- `/plan-next-increment`
- `/evaluate-gate`
- `./scripts/install-git-hooks.sh`
- `./scripts/verify-workspace.sh`

## Read on demand

| Need | File |
|---|---|
| Product contract | `docs/product-spec.md` |
| Conversion algorithm (D048) | `docs/conversion-algorithm.md` |
| Inputs and outputs | `docs/input-output-contract.md` |
| Biological semantics | `docs/compatibility-semantics.md` |
| Decisions and open forks | `docs/decisions.md` |
| Acceptance criteria | `docs/validation-contract.md` |
| Medium-scale known-truth RAD fixture plan | `docs/testing_fixture_creation.md` |
| Task plan | `TASKS.md` |
| Current state | `PROGRESS.md` |
| Resolved human product decisions | `.agent-workspace/USER_DECISIONS.md` |
| Delegation policy | `.claude/workflow.md` |

You are the orchestrator: hold the plan and approval state, delegate bounded read-only
research, synthesize findings, and own edits. Start with `.agent-workspace/KICKOFF.md`.
