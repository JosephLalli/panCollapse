#!/bin/sh
# Run the exact Ex50 fixture at several worker counts and require every persisted
# artifact to be byte-identical. The caller sets PANCOLLAPSE_MAX_CHUNK_BYTES=1
# so this also exercises ordered merging of multiple RAD chunks.
set -eu

binary=$1
gamp=$2
xg=$3
ledger=$4
output_root=$5
run_dir="$output_root/run"

snapshot() {
    output_dir=$1
    mkdir -p "$output_dir"
    for artifact in map.rad reads.bam summary.tsv tx2gene.tsv evidence.tsv; do
        cp "$run_dir/$artifact" "$output_dir/$artifact"
    done
}

# Keep the invocation paths and chunking environment exactly the same as the explicit-thread
# runs. The only difference is the omitted option, so byte equality proves the default is one.
"$binary" convert \
    --gamp "$gamp" --xg "$xg" --path-identity-ledger "$ledger" \
    --count-mode genefull_ex50pas --bam-multigene all \
    --out-dir "$run_dir" --bam-out "$run_dir/reads.bam" \
    --debug-evidence-out "$run_dir/evidence.tsv" \
    2> "$output_root/stderr_default.txt"
snapshot "$output_root/threads_default"

for threads in 1 2 4 8; do
    "$binary" convert \
        --gamp "$gamp" --xg "$xg" --path-identity-ledger "$ledger" \
        --count-mode genefull_ex50pas --bam-multigene all --threads "$threads" \
        --out-dir "$run_dir" --bam-out "$run_dir/reads.bam" \
        --debug-evidence-out "$run_dir/evidence.tsv" \
        2> "$output_root/stderr_$threads.txt"
    snapshot "$output_root/threads_$threads"
done

# Two exact models are cheaper than queue handoff and ordered retirement. Requested parallelism is
# therefore reduced to one active worker without changing any requested-thread output bytes.
grep -q 'processing workers: requested=8 active=1 reason=small_exact_model_surface exact_models=2' \
    "$output_root/stderr_8.txt"

for artifact in map.rad reads.bam summary.tsv tx2gene.tsv evidence.tsv; do
    cmp "$output_root/threads_default/$artifact" "$output_root/threads_1/$artifact"
done

for threads in 2 4 8; do
    for artifact in map.rad reads.bam summary.tsv tx2gene.tsv evidence.tsv; do
        cmp "$output_root/threads_1/$artifact" "$output_root/threads_$threads/$artifact"
    done
done
