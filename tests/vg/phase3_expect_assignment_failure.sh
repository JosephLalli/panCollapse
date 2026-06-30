#!/bin/sh
set -eu

if [ "$#" -ne 4 ]; then
  echo "usage: phase3_expect_assignment_failure.sh PANCOLLAPSE WORK_DIR ASSIGNMENT EXPECTED_MESSAGE" >&2
  exit 2
fi

pancollapse="$1"
work_dir="$2"
assignment="$3"
expected_message="$4"

if output="$("$pancollapse" convert \
  --gamp "$work_dir/reads_multi_target.gamp" \
  --xg "$work_dir/graph.xg" \
  --gtf "$work_dir/annotation_multi.gtf" \
  --collapse-manifest "$work_dir/collapse_multi.tsv" \
  --out-dir "$work_dir/pc_assignment_$assignment" \
  --raw-cb-length 16 \
  --raw-umi-length 12 \
  --assignment "$assignment" 2>&1)"; then
  echo "expected deferred assignment policy to fail: $assignment" >&2
  exit 1
fi

printf '%s\n' "$output" | grep -q -- "$expected_message"
