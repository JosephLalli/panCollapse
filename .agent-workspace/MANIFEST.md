# Removable Workspace Manifest

The following files are development-harness artifacts and can be removed after the project
no longer uses agent orchestration:

- `AGENTS.md`
- `CLAUDE.md`
- `.claude/`
- `.agent-workspace/`
- `PROGRESS.md`
- `TASKS.md`
- `.githooks/`
- `scripts/install-git-hooks.sh`
- `scripts/provenance-guard.sh`
- `scripts/remove-agent-workspace.sh`
- `scripts/verify-workspace.sh`

The following are durable project artifacts and are intentionally retained:

- `README.md`
- `LICENSE`
- `docs/product-spec.md`
- `docs/input-output-contract.md`
- `docs/compatibility-semantics.md`
- `docs/decisions.md`
- `docs/validation-contract.md`
- `docs/roadmap.md`
- `docs/glossary.md`
- research and architecture documents once they contain completed project knowledge.

The removal script deletes only the first group.
