#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
  echo "usage: phase3_expect_group_recurrence_failure.sh PANCOLLAPSE WORK_DIR" >&2
  exit 2
fi

pancollapse="$1"
work_dir="$2"

if output="$("$pancollapse" convert \
  --gamp "$work_dir/reads_recurrent.gamp" \
  --xg "$work_dir/graph.xg" \
  --gtf "$work_dir/annotation.gtf" \
  --collapse-manifest "$work_dir/collapse.tsv" \
  --out-dir "$work_dir/pc_recurrent" \
  --raw-cb-length 16 \
  --raw-umi-length 12 2>&1)"; then
  echo "expected recurrent GAMP read name to fail" >&2
  exit 1
fi

printf '%s\n' "$output" | grep -q "grouping_recurrence_failures"
