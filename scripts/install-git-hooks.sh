#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -d "$root/.git" ]]; then
  echo "Not a git repository: $root" >&2
  exit 1
fi
git -C "$root" config core.hooksPath .githooks
echo "Configured core.hooksPath=.githooks"
