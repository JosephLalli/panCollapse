#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
  echo "usage: phase3_expect_traversal_cap_failure.sh PANCOLLAPSE WORK_DIR" >&2
  exit 2
fi

pancollapse="$1"
work_dir="$2"

if output="$("$pancollapse" convert \
  --gamp "$work_dir/rpvg_reads.gamp" \
  --xg "$work_dir/rpvg_graph.xg" \
  --gtf "$work_dir/rpvg_annotation.gtf" \
  --collapse-manifest "$work_dir/rpvg_collapse.tsv" \
  --out-dir "$work_dir/pc_rpvg_traversal_cap" \
  --raw-cb-length 16 \
  --raw-umi-length 12 \
  --max-traversals-per-read 2 2>&1)"; then
  echo "expected traversal cap overflow to fail" >&2
  exit 1
fi

printf '%s\n' "$output" | grep -q "traversal_cap_exceeded_groups=1"
