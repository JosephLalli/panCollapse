#!/bin/sh
# Rebuild all test-owned inputs and outputs each time: stale artifacts cannot satisfy this test.
set -eu

binary=$1
vg=$2
python=$3
generator=$4
cmake=$5
output_root=$6
samtools=$7

"$cmake" -E remove_directory "$output_root"
"$cmake" -E make_directory "$output_root/input"
"$python" "$generator" "$output_root/input" --paths-per-parent 3 --reverse-path-order \
    --include-body-tier-read
"$vg" convert -g "$output_root/input/graph.gfa" -x > "$output_root/input/graph.xg"
"$vg" view -J -K -k "$output_root/input/reads.gamp.json" > "$output_root/input/reads.gamp"

run() {
    threads=$1
    run_dir="$output_root/run"
    "$cmake" -E remove_directory "$run_dir"
    "$binary" convert \
        --gamp "$output_root/input/reads.gamp" --xg "$output_root/input/graph.xg" \
        --path-identity-ledger "$output_root/input/path_identity_ledger.tsv" \
        --count-mode genefull_ex50pas --bam-multigene all --threads "$threads" \
        --out-dir "$run_dir" --bam-out "$run_dir/reads.bam" \
        --debug-evidence-out "$run_dir/evidence.tsv" 2> "$output_root/stderr_$threads.txt"
    "$cmake" -E copy_directory "$run_dir" "$output_root/artifacts_$threads"
}

run 1
run 8
"$cmake" -E copy_directory "$output_root/artifacts_8" "$output_root/artifacts_8_first"
run 8

# Sixty-four Parents and targets make both initialization phases use more than one worker at 8;
# reversed GFA path insertion also proves output ordering does not depend on numeric XG handles.
grep -q 'phase=exact_parent_models workers=8 parents=64' "$output_root/stderr_8.txt"
grep -q 'phase=exact_parent_models workers=8 parents=64.*models=576' "$output_root/stderr_8.txt"
grep -q 'phase=exact_parent_models workers=8 parents=64.*exon_edge_geometries=64' \
    "$output_root/stderr_8.txt"
grep -q 'phase=splice_geometry workers=8 units=64' "$output_root/stderr_8.txt"

# Geometry sharing must not collapse biological evidence identities: the exon-tier read keeps all
# three exon aliases, and the body-tier read keeps all three distinct body paths and Parents.
"$samtools" view "$output_root/artifacts_8/reads.bam" | grep '^read_000' \
    | grep -q 'XR:Z:G;X;X;X'
"$samtools" view "$output_root/artifacts_8/reads.bam" | grep '^read_000' \
    | grep -q 'GT:Z:.;E;E;E'
"$samtools" view "$output_root/artifacts_8/reads.bam" | grep '^body_tier' \
    | grep -q 'GT:Z:.;B;B;B'
"$samtools" view "$output_root/artifacts_8/reads.bam" | grep '^body_tier' \
    | grep -q 'XP:Z:PARENT_000_BODY_000,PARENT_000_BODY_001,PARENT_000_BODY_002;PARENT_000_BODY_000;PARENT_000_BODY_001;PARENT_000_BODY_002'

# Compare every file the producer persisted, including the requested debug evidence.
diff -ru "$output_root/artifacts_1" "$output_root/artifacts_8"
diff -ru "$output_root/artifacts_8_first" "$output_root/artifacts_8"

# The same graph also exercises the refactored legacy gene-geometry branch with 64 independent
# genes. This is a compatibility gate; production exact mode remains the release target.
run_legacy() {
    threads=$1
    run_dir="$output_root/legacy_run"
    "$cmake" -E remove_directory "$run_dir"
    "$binary" convert \
        --gamp "$output_root/input/reads.gamp" --xg "$output_root/input/graph.xg" \
        --legacy-adapter hst-v1 --t2g "$output_root/input/t2g.tsv" \
        --body-t2g "$output_root/input/body_t2g.tsv" --count-mode genefull \
        --threads "$threads" --out-dir "$run_dir" --bam-out "$run_dir/reads.bam" \
        2> "$output_root/legacy_stderr_$threads.txt"
    "$cmake" -E copy_directory "$run_dir" "$output_root/legacy_artifacts_$threads"
}

run_legacy 1
run_legacy 8
grep -q 'phase=splice_geometry workers=8 units=64' "$output_root/legacy_stderr_8.txt"
diff -ru "$output_root/legacy_artifacts_1" "$output_root/legacy_artifacts_8"

# Two Parents fail in different parallel blocks. Parent 000 has a deliberately long body path, so
# Parent 010 can fail first in wall-clock time; error replay must still follow lexical Parent order.
invalid_root="$output_root/invalid"
"$cmake" -E make_directory "$invalid_root/input"
"$python" "$generator" "$invalid_root/input" --invalid
"$vg" convert -g "$invalid_root/input/graph.gfa" -x > "$invalid_root/input/graph.xg"
"$vg" view -J -K -k "$invalid_root/input/reads.gamp.json" > "$invalid_root/input/reads.gamp"
for threads in 1 8; do
    "$cmake" -E remove_directory "$invalid_root/run"
    if "$binary" convert \
        --gamp "$invalid_root/input/reads.gamp" --xg "$invalid_root/input/graph.xg" \
        --path-identity-ledger "$invalid_root/input/path_identity_ledger.tsv" \
        --count-mode genefull_ex50pas --bam-multigene all --threads "$threads" \
        --out-dir "$invalid_root/run" --bam-out "$invalid_root/run/reads.bam" \
        2> "$invalid_root/stderr_$threads.txt"; then
        echo "expected parallel Parent initialization failure at --threads $threads" >&2
        exit 1
    fi
    test ! -e "$invalid_root/run"
    tail -n 1 "$invalid_root/stderr_$threads.txt" > "$invalid_root/error_$threads.txt"
done
cmp "$invalid_root/error_1.txt" "$invalid_root/error_8.txt"
grep -q 'PARENT_000_BODY and PARENT_000_EXON' "$invalid_root/error_8.txt"
