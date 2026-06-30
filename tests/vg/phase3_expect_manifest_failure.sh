#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
  echo "usage: phase3_expect_manifest_failure.sh PANCOLLAPSE WORK_DIR GAMP_FILE GTF_FILE MANIFEST_FILE EXPECTED_MESSAGE" >&2
  exit 2
fi

pancollapse="$1"
work_dir="$2"
gamp_file="$3"
gtf_file="$4"
manifest_file="$5"
expected_message="$6"

if output="$("$pancollapse" convert \
  --gamp "$work_dir/$gamp_file" \
  --xg "$work_dir/graph.xg" \
  --gtf "$work_dir/$gtf_file" \
  --collapse-manifest "$work_dir/$manifest_file" \
  --out-dir "$work_dir/pc_manifest_failure_$$" \
  --raw-cb-length 16 \
  --raw-umi-length 12 2>&1)"; then
  echo "expected manifest failure for $manifest_file" >&2
  exit 1
fi

printf '%s\n' "$output" | grep -q -- "$expected_message"
