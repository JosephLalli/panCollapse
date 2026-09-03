#!/bin/sh
# Score mode uses its own node_hst_cache. Require its persisted output to stay deterministic
# across the default, one-worker, and eight-worker paths.
set -eu

binary=$1
gamp=$2
xg=$3
t2g=$4
output_root=$5
run_dir="$output_root/run"

snapshot() {
    output_dir=$1
    mkdir -p "$output_dir"
    for artifact in map.rad reads.bam summary.tsv tx2gene.tsv; do
        cp "$run_dir/$artifact" "$output_dir/$artifact"
    done
}

"$binary" convert \
    --gamp "$gamp" --xg "$xg" --legacy-adapter hst-v1 --t2g "$t2g" \
    --count-mode score --out-dir "$run_dir" --bam-out "$run_dir/reads.bam"
snapshot "$output_root/threads_default"

for threads in 1 8; do
    "$binary" convert \
        --gamp "$gamp" --xg "$xg" --legacy-adapter hst-v1 --t2g "$t2g" \
        --count-mode score --threads "$threads" \
        --out-dir "$run_dir" --bam-out "$run_dir/reads.bam"
    snapshot "$output_root/threads_$threads"
done

for artifact in map.rad reads.bam summary.tsv tx2gene.tsv; do
    cmp "$output_root/threads_default/$artifact" "$output_root/threads_1/$artifact"
    cmp "$output_root/threads_1/$artifact" "$output_root/threads_8/$artifact"
done
