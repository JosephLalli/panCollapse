#!/bin/sh
set -eu

if [ "$#" -ne 4 ]; then
  echo "usage: phase3_expect_molecule_identity_failure.sh PANCOLLAPSE WORK_DIR GAMP_FILE EXPECTED_COUNTER" >&2
  exit 2
fi

pancollapse="$1"
work_dir="$2"
gamp_file="$3"
expected_counter="$4"

if output="$("$pancollapse" convert \
  --gamp "$work_dir/$gamp_file" \
  --xg "$work_dir/graph.xg" \
  --gtf "$work_dir/annotation.gtf" \
  --collapse-manifest "$work_dir/collapse.tsv" \
  --out-dir "$work_dir/pc_bad_molecule_strict_$expected_counter" \
  --raw-cb-length 16 \
  --raw-umi-length 12 \
  --molecule-identity-failures fail 2>&1)"; then
  echo "expected strict raw molecule identity mode to fail" >&2
  exit 1
fi

printf '%s\n' "$output" | grep -q "$expected_counter=1"
