# panCollapse — Agent Index

Mission: design and implement the product specified in `docs/product-spec.md` without
reopening settled decisions or silently resolving open biological or interface questions.
This file is intentionally short; load detailed documents only when relevant.

## Always apply

- Treat `docs/product-spec.md` as the product contract.
- Verify external APIs and file-format assumptions from primary sources before designing.
- Do not write production code until Gate Architecture Approved has passed.
- Prefer using standard vg indexes; a custom lookup index is not part of V1.
- Update `PROGRESS.md` and `docs/decisions.md` before ending a substantive session.
- Stop for human review wherever correctness depends on biological or product judgment.

## Read on demand

| Need | File |
|---|---|
| Product contract | `docs/product-spec.md` |
| Input/output contract | `docs/input-output-contract.md` |
| Transcript compatibility semantics | `docs/compatibility-semantics.md` |
| Settled decisions and open forks | `docs/decisions.md` |
| Validation and acceptance criteria | `docs/validation-contract.md` |
| Staged implementation plan | `TASKS.md` |
| Current state | `PROGRESS.md` |
| Delegation policy | `.agent-workspace/ORCHESTRATION.md` |
| Development gates | `.agent-workspace/GATES.md` |
| Resolved human product decisions | `.agent-workspace/USER_DECISIONS.md` |

## Session protocol

1. Read `PROGRESS.md` and the relevant task card.
2. Confirm the current named development gate.
3. Delegate context-heavy research to the read-only agents in `.claude/agents/` when
   available; otherwise perform the same bounded research directly.
4. Produce concise artifacts, not raw source dumps.
5. Run only work permitted by the current gate.
6. Record decisions and leave an explicit next step.
