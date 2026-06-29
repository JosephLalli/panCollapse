# Model Policy

Agent definitions use `model: inherit` for portability across Codex, Claude Code, and
future platforms.

At session start, map roles to the current platform:

- narrow source search and file mapping: lowest-cost model that reliably follows citations;
- multi-repository API synthesis: strong general coding/reasoning model;
- architecture, biological semantics, and gate review: strongest available reasoning
  model;
- production implementation after approval: strong coding model with full parent-session
  permissions.

Subagents remain read-only. Writes, commands requiring approval, commits, and named-gate
state stay with the orchestrator.
