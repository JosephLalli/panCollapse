# Orchestration Protocol

## Authority

The parent session is the sole orchestrator. It owns integration of repository edits, the
task state, interface synthesis, and requests for human approval. It assigns bounded write
ownership for documentation checkpoints; researchers otherwise remain read-only.

## Scientific phase documentation checkpoint

User instruction, 2026-09-15: before entering any new scientific phase, automatically run
a documentation agent instead of offering `/reload`. Complete and review the update before
beginning the phase; this documentation work needs no separate confirmation.

Give the agent explicit ownership of the affected local AI-facing documents, starting from
README's Current work chain, `PROGRESS.md`, the relevant decision records, and recent research
records. Reconcile existing documents and navigation rather than creating a duplicate index.
The update must make these discoverable:

- intellectual state: goals, current understanding, hypotheses, and unresolved questions;
- generated files and experiments: exact input/output locations, executed versus pending
  work, results, validation evidence, and limits on interpretation;
- decisions: accepted rules, superseded conclusions, open human judgments, and current gate;
- code: relevant source/worktree/branch, existing implementation, tested executable identity,
  and checks already performed;
- continuation: the next authorized bounded task and where to find its supporting evidence.

Preserve existing edits, completed outputs, and historical records. Use recorded terminal
receipts and targeted checks; documentation reconciliation does not imply rerunning an
experiment. Keep converter performance, replay performance, and counting accuracy separate.
The agent reports changed files, evidence locations, checks, and unresolved documentation gaps.
The parent reviews that report before proceeding within the user's existing authorization.
This checkpoint cannot resolve an unapproved biological or product decision.

## Parallel Phase 0 tracks

The following investigations are independent and may run in parallel:

1. VG integration and build/linkage.
2. GAMP grouping and sorting.
3. RAD format and writer strategy.
4. GTF/graph compatibility lookup.

Researchers must not edit shared documents. The orchestrator synthesizes their concise
reports into the canonical research briefs and architecture proposal.

## Interface changes

Any change to these shared contracts must be routed through the decision log:

- required inputs (GAMP, `.xg` with HST paths, t2g);
- barcode/UMI source policy;
- compatibility semantics (HST-path node membership);
- node scoring and HST attribution;
- RAD output contract;
- V1 scope.

Note: the collapse-manifest schema and assignment-policy items that appeared here before
D048 are removed; those surfaces no longer exist in the active design.

## Context budget

Do not paste large source files or logs into the main session. Researchers return exact
locations and minimal excerpts. Persist durable findings to the research briefs, decisions,
and progress file.

## End-of-session requirement

Every substantive session leaves:

- current phase and gate;
- work completed;
- evidence or commands used;
- unresolved forks;
- exact next task;
- any required human decision.
