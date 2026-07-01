# panCollapse — Agent Index

Design and implement the GAMP-to-RAD product specified in `docs/product-spec.md`. This is
an index, not the full manual; read detailed files only when working in their area.

## Principles

- Verify external APIs and formats before designing against them.
- Do not write production code until Gate Architecture Approved has passed.
- Preserve multimapping evidence. Active GAMP-to-RAD output emits all compatible retained
  RAD targets after compatibility, collapse, and score filtering; transcript- and
  gene-uniqueness modes are deferred/to-be-implemented.
- Use existing VG/index and GTF inputs in V1; do not introduce a custom lookup index
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
