#!/usr/bin/env sh
set -eu

if [ "$#" -ne 2 ]; then
  echo "usage: phase2_expect_strand_tbi.sh PANCOLLAPSE STRAND" >&2
  exit 2
fi

pancollapse="$1"
strand="$2"

if output="$("$pancollapse" convert \
  --gamp missing.gamp \
  --xg missing.xg \
  --gtf missing.gtf \
  --collapse-manifest missing.tsv \
  --out-dir missing_out \
  --strand "$strand" 2>&1)"; then
  echo "expected --strand $strand to fail in Phase 2" >&2
  exit 1
fi

printf '%s\n' "$output" | grep -q -- "--strand $strand is to be implemented in Phase 3"
