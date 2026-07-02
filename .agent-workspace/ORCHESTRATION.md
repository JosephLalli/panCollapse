# Orchestration Protocol

## Authority

The parent session is the sole orchestrator. It owns repository edits, the task state,
interface synthesis, and requests for human approval.

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
