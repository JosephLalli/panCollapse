#!/bin/sh
set -eu

if [ "$#" -ne 5 ]; then
  echo "usage: phase3_expect_cli_numeric_failure.sh PANCOLLAPSE WORK_DIR OPTION VALUE EXPECTED_MESSAGE" >&2
  exit 2
fi

pancollapse="$1"
work_dir="$2"
option="$3"
value="$4"
expected_message="$5"

if output="$("$pancollapse" convert \
  --gamp "$work_dir/reads.gamp" \
  --xg "$work_dir/graph.xg" \
  --gtf "$work_dir/annotation.gtf" \
  --collapse-manifest "$work_dir/collapse.tsv" \
  --out-dir "$work_dir/pc_invalid_numeric_$$" \
  --raw-cb-length 16 \
  --raw-umi-length 12 \
  "$option" "$value" 2>&1)"; then
  echo "expected invalid numeric option to fail: $option $value" >&2
  exit 1
fi

printf '%s\n' "$output" | grep -q -- "$expected_message"
