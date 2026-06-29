#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dry_run=0
if [[ "${1:-}" == "--dry-run" ]]; then
  dry_run=1
elif [[ $# -gt 0 ]]; then
  echo "Usage: $0 [--dry-run]" >&2
  exit 2
fi

paths=(
  AGENTS.md
  CLAUDE.md
  .claude
  .agent-workspace
  PROGRESS.md
  TASKS.md
  .githooks
  scripts/install-git-hooks.sh
  scripts/provenance-guard.sh
  scripts/remove-agent-workspace.sh
  scripts/verify-workspace.sh
)

for rel in "${paths[@]}"; do
  if [[ -e "$root/$rel" || -L "$root/$rel" ]]; then
    if [[ "$dry_run" -eq 1 ]]; then
      printf '%s\n' "$rel"
    else
      rm -rf -- "$root/$rel"
    fi
  fi
done

if [[ "$dry_run" -eq 0 ]]; then
  echo "Removed tracked agent workspace; durable project documents remain."
fi
