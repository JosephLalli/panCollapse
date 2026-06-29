# Provenance rules

Agent-facing workspace files are intentionally tracked. Production code, tests, build
files, release notes, and user-facing runtime output must not contain tool/model
attribution, automated-generation claims, or co-author trailers.

Run `./scripts/provenance-guard.sh` before committing production changes. Documentation,
AGENTS.md, CLAUDE.md, `.claude/`, and `.agent-workspace/` are exempt because they are the
explicit tracked workspace layer.
