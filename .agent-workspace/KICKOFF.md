# Initial Kickoff

Use this prompt for the first Codex, Claude Code, or comparable coding-agent session:

> Read `AGENTS.md`, `PROGRESS.md`, `TASKS.md`, `docs/product-spec.md`,
> `docs/compatibility-semantics.md`, `docs/decisions.md`, and `.agent-workspace/USER_DECISIONS.md`. Execute Phase 0 only.
> Delegate the four external-contract investigations to isolated read-only researchers
> where supported. Verify current behavior from primary sources or installed source,
> record version/commit evidence, and synthesize a concrete architecture and CLI proposal.
> Update the four research briefs, `docs/architecture-proposal.md`, `docs/decisions.md`,
> and `PROGRESS.md`. Do not create production source, CMake files, fixtures, or RAD output.
> Stop at Gate Architecture Approved and provide the named-gate checklist for human approval. Treat all choices in `.agent-workspace/USER_DECISIONS.md` as settled and do not reopen them.

The first phase is deliberately bounded because VG linkage, multipath traversal, GAMP
emission, coordinate lookup, and RAD serialization are external contracts that must be
verified before implementation.
