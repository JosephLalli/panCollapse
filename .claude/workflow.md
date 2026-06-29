# Delegation Workflow

One orchestrator owns the plan, repository edits, decisions, and development-gate status.
Subagents are read-only investigators or reviewers and return concise evidence-backed
reports.

## Delegate when

- the task requires reading substantial VG, alevin-fry, or format source;
- large search results would pollute the main context;
- an independent quality review is useful;
- the task recurs across sessions.

## Do not delegate when

- the task is a small edit with no separate context burden;
- workers would independently change the same interface;
- a human judgment call is required.

## Shared-interface protocol

Any proposed change to the product spec, manifest schema, RAD contract, CLI, or fixture
expectations must be synthesized by the orchestrator and recorded in `docs/decisions.md`
before implementation. Researchers recommend; they do not silently settle interfaces.

## Return shape

Researchers return:

1. verdict;
2. primary-source evidence with version/commit;
3. uncertainties;
4. recommendation;
5. smallest verification experiment.

Reviewers return:

1. pass/fail by contract section;
2. specific defects;
3. missing tests;
4. whether the current development gate can advance.

## Model policy

Use the platform's current low-cost capable model for narrow source searches, a stronger
model for multi-source synthesis, and the strongest available reasoning model for
architecture or biological correctness review. Agent definitions use `inherit` to remain
portable; the orchestrator may override based on the current platform.
