#!/usr/bin/env bash
# Run the authorized 1,000,000-read HG002 chr20+chr21+chr22 PanCollapse
# gene-assignment benchmark. Selection is already frozen in SUBSET_ROOT.
#
# The correction stage intentionally reads the complete producer BAM twice:
# pass 1 learns the frozen all-read barcode prior, while pass 2 checks the frozen
# selected QNAME set before correcting and writing records. This avoids both a
# subset-derived prior and work or serialization for unselected records.

set -Eeuo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
readonly SCRIPT_DIR
readonly SCRIPT_VERSION="1.3.2"
readonly SCHEMA="pansc-hg002-joint-v090-assignment-benchmark-1m-v1"
readonly SAMPLE_SIZE="1000000"
readonly SEED="pansc-hg002-joint-v090-gene-assignment-1m-v1"

readonly DEFAULT_ROOT="/mnt/ssd/lalli/hg002_chr20_chr21_chr22_pancollapse_v090_gene_assignment_1m_v1_20260904T171055Z"
readonly SOURCE_ROOT="/mnt/ssd/lalli/hg002_chr20_chr21_chr22_pancollapse_v090_full_v1_20260903T215515Z"
readonly SOURCE_BAM="${SOURCE_ROOT}/pangenome/run/pancollapse/genetag.bam"
readonly PRODUCER_CHECKSUMS="${SOURCE_ROOT}/producer.FULL.SHA256SUMS"
readonly SOURCE_BAM_BYTES="167672788712"
readonly SOURCE_BAM_SHA256="74599094a05b86c376ce92c1c096e118556ccf6307fbbbe678f16e5cfb62e2dc"
readonly PRODUCER_CHECKSUMS_SHA256="df2e47b0221af7306b435fe012029eeff2ec9ea30e7908e12d6d32afc5927aff"

readonly TRUTH_ROOT="/mnt/ssd/lalli/hg002_chr20_chr21_chr22_q100_simulation_full_v1_20260831T131151-0500"
readonly TEMPLATE_TRUTH="${TRUTH_ROOT}/template_truth.tsv"
readonly TEMPLATE_TRUTH_SHA256="0387959ceb4f99ddb7ff442959d33918c064c048655ca71117b926390b8f7606"
readonly TRUTH_CHECKSUMS_SHA256="dbc1d1b40c3c7a439a02fef26eae79e67200009c45424313f8b12c1cf13f8a01"

readonly FREEZE_ROOT="/mnt/ssd/lalli/hg002_chr20_chr21_chr22_strict_membership_final_method_freeze_v8_v090_20260904T055700Z"
readonly FREEZE_MANIFEST="${FREEZE_ROOT}/MANIFEST.json"
readonly FREEZE_MANIFEST_SHA256="71e1b5c401cd5a89f9300cd1a1aa54974f37d51d81bbcd50b314575ac7d64d1d"
readonly FROZEN_RUNTIME="${FREEZE_ROOT}/provenance"
readonly CORRECT_CB="${FROZEN_RUNTIME}/bin/correct_cb.py"
readonly COUNT_CR="${FROZEN_RUNTIME}/bin/count_cr.py"
readonly SCORE_READS="${FROZEN_RUNTIME}/scripts/score_count_cr_read_classification.py"
readonly CORRECT_CB_SELECTED="${SCRIPT_DIR}/correct_cb_selected.py"
readonly CORRECT_CB_SHA256="a54ac84bf757e779e9591792f6e387d241acbbdd0f8d40be1590ae3c6a83b526"
readonly COUNT_CR_SHA256="4d9707ef7c62b2b7db8741ffe824949904fbffc5c03268e496ced4b4a9ab5104"
readonly SCORE_READS_SHA256="0382efb093cc750a96a6e7a9542f5d8c41a2390ee3ed5dad439b948f1b943bca"
readonly CORRECT_CB_SELECTED_SHA256="487aa67d191523e63bb94d0e04e7f32e497ca7ec170f5ceaa8539b7326302e0a"

readonly ANNOTATION_ROOT="/mnt/ssd/lalli/hg002_chr20_chr21_chr22_strict_membership_annotation_v1_20260903T063000Z"
readonly PATH_LEDGER="${ANNOTATION_ROOT}/path_identity_ledger.corrected.tsv"
readonly PARENT_CATEGORIES="${ANNOTATION_ROOT}/parent_categories/parent_categories.tsv"
readonly PATH_LEDGER_SHA256="73c7a7a03abcbb9eb795c3ac2b1141cca82333c73134b190ee1a30be8657f10c"
readonly PARENT_CATEGORIES_SHA256="157cfae75312f1d89cfcff9e6e64088daa718013a530793f5420f8fd50b27520"

readonly POLICY_ROOT="/mnt/ssd/lalli/hg002_chr20_chr21_chr22_strict_membership_final_v082_v1_20260903T153417Z"
readonly DIRECT_POLICY="${POLICY_ROOT}/direct_policy/gene_policy.tsv.gz"
readonly PROJECTED_POLICY="${POLICY_ROOT}/projected_policy/gene_policy.tsv.gz"
readonly DIRECT_POLICY_SHA256="2c90861aa6259c2aa04e15730bf9d28bd479e6cfa27d540f0f62ab7acbde623f"
readonly PROJECTED_POLICY_SHA256="337c8749b6791e6e7a5372f3c587b1004ad4b0508ea7c9bd7044d00fbde6ea2b"

readonly WHITELIST="/mnt/ssd/lalli/panSC/tests/fixtures/mhc/3M-february-2018.txt"
readonly WHITELIST_SHA256="843a6f7038db8cb3c06f3dc21cc69d04139ffa10689780518d7a9e42dc2e819b"
readonly PYSAM_IMAGE="quay.io/biocontainers/pysam:0.24.0--py311h5f69268_0"
readonly PYSAM_IMAGE_ID="sha256:2aa2864c82aeabf7294723f1c979ea680b112948adbe0719167b449dfbca5fd7"
readonly HOST_PYTHON="/mnt/ssd/lalli/.linuxbrew/Cellar/python@3.11/3.11.14/bin/python3.11"

ROOT="${DEFAULT_ROOT}"
PREFLIGHT_ONLY=0
RESUME_AFTER_CORRECTION=0

usage() {
    cat <<EOF
Usage: $(basename "$0") [--root PATH] [--preflight-only] [--resume-after-correction]

Runs the fixed, authorized partial-unblinding benchmark. Existing partial or
terminal benchmark directories are never overwritten. The narrowly scoped
resume mode accepts only a structurally valid, record-count-verified correction
artifact whose producer command exited successfully before wrapper validation.
EOF
}

while (($#)); do
    case "$1" in
        --root)
            (($# >= 2)) || { usage >&2; exit 2; }
            ROOT="$2"
            shift 2
            ;;
        --preflight-only)
            PREFLIGHT_ONLY=1
            shift
            ;;
        --resume-after-correction)
            RESUME_AFTER_CORRECTION=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'ERROR: unknown argument: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

readonly ROOT
readonly SUBSET_ROOT="${ROOT}/subset"
readonly SELECTED_NAMES="${SUBSET_ROOT}/selected_read_names.txt"
readonly SUBSET_FASTQ="${SUBSET_ROOT}/R2.subset.fastq.gz"
readonly LOG_ROOT="${ROOT}/logs"
readonly COMMAND_ROOT="${ROOT}/commands"
readonly TIMING_ROOT="${ROOT}/timings"
readonly RUN_STATUS="${ROOT}/BENCHMARK.STATUS"

(( ! (PREFLIGHT_ONLY && RESUME_AFTER_CORRECTION) )) || {
    printf 'ERROR: --preflight-only and --resume-after-correction are mutually exclusive\n' >&2
    exit 2
}

CURRENT_STAGE="preflight"

timestamp() {
    date --utc '+%Y-%m-%dT%H:%M:%SZ'
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    return 1
}

on_error() {
    local exit_code=$?
    trap - ERR
    if [[ -d "${ROOT}" && ${PREFLIGHT_ONLY} -eq 0 ]]; then
        printf 'benchmark_failed\tstage=%s\texit_code=%s\tutc=%s\n' \
            "${CURRENT_STAGE}" "${exit_code}" "$(timestamp)" > "${RUN_STATUS}"
    fi
    printf 'FAILED: stage=%s exit_code=%s\n' "${CURRENT_STAGE}" "${exit_code}" >&2
    exit "${exit_code}"
}
trap on_error ERR

require_file() {
    [[ -f "$1" ]] || fail "missing file: $1"
}

require_text() {
    local observed
    observed=$(<"$1")
    [[ "${observed}" == "$2" ]] || fail "$1 has unexpected content: ${observed}"
}

require_sha256() {
    local observed
    observed=$(sha256sum "$1")
    observed=${observed%% *}
    [[ "${observed}" == "$2" ]] || fail "$1 SHA-256 ${observed}, expected $2"
}

require_manifest_entry() {
    local manifest=$1
    local relative=$2
    local expected=$3
    local observed
    observed=$(awk -v target="${relative}" '$2 == target {print $1}' "${manifest}")
    [[ "${observed}" == "${expected}" ]] || \
        fail "${manifest} entry ${relative} was ${observed:-missing}, expected ${expected}"
}

record_command() {
    local path=$1
    shift
    {
        printf 'cwd=%q\n' "${PWD}"
        printf 'command='
        printf '%q ' "$@"
        printf '\n'
    } > "${path}"
}

summary_value() {
    local key=$1
    local line=$2
    local field
    for field in ${line}; do
        case "${field}" in
            "${key}="*)
                printf '%s\n' "${field#*=}"
                return 0
                ;;
        esac
    done
    fail "missing ${key} in correction summary"
}

preflight() {
    local available_kib image_id name_lines bam_bytes

    for executable in awk df docker gzip samtools sha256sum stat wc; do
        command -v "${executable}" >/dev/null || fail "missing executable: ${executable}"
    done
    require_file "${HOST_PYTHON}"
    require_sha256 "${FREEZE_MANIFEST}" "${FREEZE_MANIFEST_SHA256}"
    "${HOST_PYTHON}" - "${FREEZE_MANIFEST}" <<'PY'
import json
import pathlib
import sys

manifest = json.loads(pathlib.Path(sys.argv[1]).read_text())
assert manifest["method"]["optimization_objective"] == "uniform_truth_gene_f1"
assert manifest["method"]["metric_contract"]["traditional_read_weighted_metrics"] == "mandatory co-report"
PY
    require_file "${SUBSET_ROOT}/STATUS"
    require_file "${SUBSET_ROOT}/MANIFEST.json"
    require_file "${SUBSET_ROOT}/SHA256SUMS"
    require_file "${SELECTED_NAMES}"
    require_file "${SUBSET_FASTQ}"
    awk -F '\t' '
        $1 == "complete" && $2 == "selected_reads=1000000" &&
        $3 ~ /^completed_utc=/ {matched = 1}
        END {exit !matched}
    ' "${SUBSET_ROOT}/STATUS" || fail "subset STATUS is not terminal for 1,000,000 reads"
    (cd "${SUBSET_ROOT}" && sha256sum --check --quiet SHA256SUMS)

    "${HOST_PYTHON}" - "${SUBSET_ROOT}/MANIFEST.json" <<'PY'
import json
import sys

manifest = json.load(open(sys.argv[1], encoding="utf-8"))
assert manifest["schema"] == "pansc-assignment-blind-read-subset-v1"
assert manifest["status"] == "complete"
selection = manifest["selection"]
assert selection["assignment_blind"] is True
assert selection["sample_size"] == 1_000_000
assert selection["seed"] == "pansc-hg002-joint-v090-gene-assignment-1m-v1"
assert manifest["truth_composition"]["reads_by_mapping_status"] == {"mapped": 1_000_000}
assert sum(manifest["truth_composition"]["reads_by_scope"].values()) == 1_000_000
assert manifest["inputs"]["source_bam"]["sha256"] == "74599094a05b86c376ce92c1c096e118556ccf6307fbbbe678f16e5cfb62e2dc"
PY
    name_lines=$(wc -l < "${SELECTED_NAMES}")
    [[ "${name_lines}" == "${SAMPLE_SIZE}" ]] || \
        fail "selected-name count ${name_lines}, expected ${SAMPLE_SIZE}"

    require_file "${SOURCE_BAM}"
    require_file "${PRODUCER_CHECKSUMS}"
    awk -F '\t' '
        $1 == "pangenome_complete" && $3 == "reads=11881577" {matched = 1}
        END {exit !matched}
    ' "${SOURCE_ROOT}/pangenome/run/STATUS" || \
        fail "producer STATUS is not terminal for 11,881,577 reads"
    require_sha256 "${PRODUCER_CHECKSUMS}" "${PRODUCER_CHECKSUMS_SHA256}"
    require_manifest_entry "${PRODUCER_CHECKSUMS}" "pancollapse/genetag.bam" "${SOURCE_BAM_SHA256}"
    bam_bytes=$(stat --format='%s' "${SOURCE_BAM}")
    [[ "${bam_bytes}" == "${SOURCE_BAM_BYTES}" ]] || \
        fail "source BAM size ${bam_bytes}, expected ${SOURCE_BAM_BYTES}"

    require_file "${TRUTH_ROOT}/SHA256SUMS"
    require_file "${TEMPLATE_TRUTH}"
    require_sha256 "${TRUTH_ROOT}/SHA256SUMS" "${TRUTH_CHECKSUMS_SHA256}"
    require_manifest_entry "${TRUTH_ROOT}/SHA256SUMS" "template_truth.tsv" "${TEMPLATE_TRUTH_SHA256}"

    require_text "${FREEZE_ROOT}/STATUS" \
        $'joint_final_method_frozen_pre_unblinding\tsha256:b60634bd04695b6eaa0d6498e2dd7aba2a3d21248ef8b2760eda40438e265b67'
    (cd "${FREEZE_ROOT}" && sha256sum --check --quiet SHA256SUMS)
    require_sha256 "${CORRECT_CB}" "${CORRECT_CB_SHA256}"
    require_sha256 "${COUNT_CR}" "${COUNT_CR_SHA256}"
    require_sha256 "${SCORE_READS}" "${SCORE_READS_SHA256}"
    require_sha256 "${CORRECT_CB_SELECTED}" "${CORRECT_CB_SELECTED_SHA256}"

    require_file "${ANNOTATION_ROOT}/SHA256SUMS"
    require_manifest_entry "${ANNOTATION_ROOT}/SHA256SUMS" \
        "./path_identity_ledger.corrected.tsv" "${PATH_LEDGER_SHA256}"
    require_manifest_entry "${ANNOTATION_ROOT}/SHA256SUMS" \
        "./parent_categories/parent_categories.tsv" "${PARENT_CATEGORIES_SHA256}"
    require_file "${PATH_LEDGER}"
    require_file "${PARENT_CATEGORIES}"
    require_sha256 "${DIRECT_POLICY}" "${DIRECT_POLICY_SHA256}"
    require_sha256 "${PROJECTED_POLICY}" "${PROJECTED_POLICY_SHA256}"
    require_sha256 "${WHITELIST}" "${WHITELIST_SHA256}"

    image_id=$(docker image inspect --format '{{.Id}}' "${PYSAM_IMAGE}")
    [[ "${image_id}" == "${PYSAM_IMAGE_ID}" ]] || \
        fail "pysam image ID ${image_id}, expected ${PYSAM_IMAGE_ID}"

    available_kib=$(df --output=avail "${ROOT}" | awk 'NR == 2 {print $1}')
    [[ "${available_kib}" =~ ^[0-9]+$ ]] || fail "could not measure available disk"
    (( available_kib >= 100 * 1024 * 1024 )) || \
        fail "less than 100 GiB free at ${ROOT}"
}

run_correction() {
    local partial="${ROOT}/correct_barcodes.partial"
    local final="${ROOT}/correct_barcodes"
    local started ended
    local -a correction_command

    [[ ! -e "${partial}" ]] || fail "preserving existing partial stage: ${partial}"
    [[ ! -e "${final}" ]] || fail "refusing existing terminal stage: ${final}"
    mkdir "${partial}"
    correction_command=(
        docker run --rm --user "$(id -u):$(id -g)"
        -v /mnt/ssd/lalli:/mnt/ssd/lalli
        "${PYSAM_IMAGE}" python3 "${CORRECT_CB_SELECTED}"
        "${SOURCE_BAM}" "${partial}/cbcorr.subset.bam" "${WHITELIST}"
        "${SELECTED_NAMES}"
        --expected-source-records 11881577
        --expected-selected-records "${SAMPLE_SIZE}"
        --progress-every 1000000
        --filter-threads 8
        --output-threads 8
    )
    record_command "${COMMAND_ROOT}/correct_barcodes.txt" "${correction_command[@]}"
    started=$(timestamp)
    printf 'correct_barcodes_running\tstarted_utc=%s\n' "${started}" > "${partial}/STATUS"
    /usr/bin/time --verbose --output="${TIMING_ROOT}/correct_barcodes.resource.txt" \
        "${correction_command[@]}" \
        > "${LOG_ROOT}/correct_barcodes.stdout.log" \
        2> "${LOG_ROOT}/correct_barcodes.stderr.log"
    # samtools quickcheck in the host release accepts short options only and
    # does not recognize the conventional `--` end-of-options marker.
    samtools quickcheck -v "${partial}/cbcorr.subset.bam"
    ended=$(timestamp)
    printf 'correct_barcodes_complete\tstarted_utc=%s\tcompleted_utc=%s\n' \
        "${started}" "${ended}" > "${partial}/STATUS"
    mv "${partial}" "${final}"
}

resume_completed_correction() {
    local partial="${ROOT}/correct_barcodes.partial"
    local final="${ROOT}/correct_barcodes"
    local bam="${partial}/cbcorr.subset.bam"
    local correction_log="${LOG_ROOT}/correct_barcodes.stderr.log"
    local resource_log="${TIMING_ROOT}/correct_barcodes.resource.txt"
    local receipt="${ROOT}/CORRECTION_RESUME.tsv"
    local failed_status started summary source_records selected_records
    local selected_filter_records emitted dropped_uncorrectable barcode_only
    local observed_records bam_bytes

    [[ -d "${partial}" ]] || fail "resume requires ${partial}"
    [[ ! -e "${final}" ]] || fail "resume refuses existing terminal stage: ${final}"
    [[ ! -e "${receipt}" ]] || fail "resume receipt already exists: ${receipt}"
    require_file "${bam}"
    require_file "${correction_log}"
    require_file "${resource_log}"
    require_file "${RUN_STATUS}"

    failed_status=$(<"${RUN_STATUS}")
    [[ "${failed_status}" == benchmark_failed$'\t'stage=correct_barcodes$'\t'* ]] || \
        fail "resume requires a recorded correction-stage wrapper failure"
    grep -Eq '^[[:space:]]*Exit status: 0$' "${resource_log}" || \
        fail "correction producer did not record exit status 0"

    summary=$(awk '/^source_records=/ {line = $0} END {print line}' "${correction_log}")
    [[ -n "${summary}" ]] || fail "correction producer terminal summary is missing"
    source_records=$(summary_value source_records "${summary}")
    selected_records=$(summary_value selected_records "${summary}")
    selected_filter_records=$(summary_value selected_filter_records "${summary}")
    emitted=$(summary_value emitted "${summary}")
    dropped_uncorrectable=$(summary_value dropped_uncorrectable "${summary}")
    barcode_only=$(summary_value barcode_only "${summary}")
    [[ "${source_records}" == "11881577" ]] || fail "unexpected source record count"
    [[ "${selected_records}" == "${SAMPLE_SIZE}" ]] || fail "unexpected selected-name count"
    [[ "${selected_filter_records}" == "${SAMPLE_SIZE}" ]] || \
        fail "selected htslib filter did not return the complete subset"
    (( emitted + dropped_uncorrectable + barcode_only == SAMPLE_SIZE )) || \
        fail "correction terminal accounting does not reconcile"

    samtools quickcheck -v "${bam}"
    observed_records=$(samtools view -@ 8 -c "${bam}")
    [[ "${observed_records}" == "${emitted}" ]] || \
        fail "corrected BAM has ${observed_records} records, expected ${emitted}"
    bam_bytes=$(stat -c '%s' "${bam}")
    started=$(awk -F '\t' 'NR == 1 {sub(/^started_utc=/, "", $2); print $2}' \
        "${partial}/STATUS")
    [[ -n "${started}" ]] || fail "correction start time is missing"

    {
        printf 'schema\tpansc-correction-wrapper-resume-v1\n'
        printf 'reason\tsamtools_quickcheck_short_option_compatibility\n'
        printf 'prior_benchmark_status\t%s\n' "${failed_status}"
        printf 'producer_exit_status\t0\n'
        printf 'source_records\t%s\n' "${source_records}"
        printf 'selected_filter_records\t%s\n' "${selected_filter_records}"
        printf 'emitted_records\t%s\n' "${emitted}"
        printf 'corrected_bam_bytes\t%s\n' "${bam_bytes}"
        printf 'quickcheck\tpass\n'
        printf 'record_count_check\tpass\n'
        printf 'runner_version\t%s\n' "${SCRIPT_VERSION}"
        printf 'resumed_utc\t%s\n' "$(timestamp)"
    } > "${receipt}"
    printf 'correct_barcodes_complete\tstarted_utc=%s\tcompleted_utc=%s\tresumed_after_wrapper_validation=true\n' \
        "${started}" "$(timestamp)" > "${partial}/STATUS"
    mv "${partial}" "${final}"
}

run_count() {
    local partial="${ROOT}/count.partial"
    local final="${ROOT}/count"
    local corrected="${ROOT}/correct_barcodes/cbcorr.subset.bam"
    local started ended
    local -a command

    [[ ! -e "${partial}" ]] || fail "preserving existing partial stage: ${partial}"
    [[ ! -e "${final}" ]] || fail "refusing existing terminal stage: ${final}"
    require_file "${corrected}"
    mkdir "${partial}"
    command=(
        docker run --rm --user "$(id -u):$(id -g)"
        -v /mnt/ssd/lalli:/mnt/ssd/lalli
        "${PYSAM_IMAGE}" python3 "${COUNT_CR}"
        "${corrected}" "${partial}/counts"
        --analysis-scope sensitivity-analysis
        --cr7
        --tagged-last-resort
        --strand forward
        --require-superset-schema
        --path-identity-ledger "${PATH_LEDGER}"
        --gene-policy-ledger "${PROJECTED_POLICY}"
        --parent-category-ledger "${PARENT_CATEGORIES}"
        --read-classification-out "${partial}/read_classification.tsv.gz"
        --multigene-identities-out "${partial}/multigene_identities.tsv.gz"
        --gene-competition-audit-out "${partial}/gene_competition_audit.tsv.gz"
        --exact-strand-gene-fallback
        --cr7-exclude-transcript-tag candidate_novel_locus
        --cr7-exclude-transcript-tag foreign_component_only_except_host_concordant_small_rna
        --cr7-exclude-transcript-tag no_source_provenance
        --cr7-exclude-transcript-tag overlaps_pseudogene
        --cr7-exclude-transcript-tag readthrough_transcript
        --cr7-exclude-transcript-tag source_gene_label_conflict
        --cr7-exclude-transcript-tag structural_readthrough
        --post-resolution-empty-fallback-category no_source_provenance
        --direct-multigene-body-sample-ratio 4
        --direct-multigene-body-winner-gene-type protein_coding
        --candidate-strong-support-exemption
        --candidate-strong-support-audit-out "${partial}/strong_support_parent_audit.tsv.gz"
    )
    record_command "${COMMAND_ROOT}/count.txt" "${command[@]}"
    started=$(timestamp)
    printf 'count_running\tstarted_utc=%s\n' "${started}" > "${partial}/STATUS"
    /usr/bin/time --verbose --output="${TIMING_ROOT}/count.resource.txt" \
        "${command[@]}" \
        > "${LOG_ROOT}/count.stdout.log" \
        2> "${LOG_ROOT}/count.stderr.log"
    require_file "${partial}/read_classification.tsv.gz"
    gzip --test "${partial}/read_classification.tsv.gz"
    require_file "${partial}/counts/alevin/quants_mat.mtx"
    ended=$(timestamp)
    printf 'count_complete\tstarted_utc=%s\tcompleted_utc=%s\n' \
        "${started}" "${ended}" > "${partial}/STATUS"
    mv "${partial}" "${final}"
}

run_score() {
    local partial="${ROOT}/score.partial"
    local final="${ROOT}/score"
    local classification="${ROOT}/count/read_classification.tsv.gz"
    local started ended
    local -a command

    [[ ! -e "${partial}" ]] || fail "preserving existing partial stage: ${partial}"
    [[ ! -e "${final}" ]] || fail "refusing existing terminal stage: ${final}"
    require_file "${classification}"
    command=(
        "${HOST_PYTHON}" "${SCORE_READS}"
        --template-truth "${TEMPLATE_TRUTH}"
        --reads-fastq "${SUBSET_FASTQ}"
        --eligibility-gene-policy "${DIRECT_POLICY}"
        --scope-column scope_chromosome
        --classification "pancollapse=${classification}"
        --out-dir "${partial}"
    )
    record_command "${COMMAND_ROOT}/score.txt" "${command[@]}"
    started=$(timestamp)
    /usr/bin/time --verbose --output="${TIMING_ROOT}/score.resource.txt" \
        "${command[@]}" \
        > "${LOG_ROOT}/score.stdout.log" \
        2> "${LOG_ROOT}/score.stderr.log"
    (cd "${partial}" && sha256sum --check --quiet SHA256SUMS)
    ended=$(timestamp)
    # Preserve the scorer's own terminal STATUS inside its immutable directory;
    # stage timing is recorded separately.
    printf 'score_complete\tstarted_utc=%s\tcompleted_utc=%s\n' \
        "${started}" "${ended}" > "${TIMING_ROOT}/score.STATUS"
    mv "${partial}" "${final}"
}

finalize_benchmark() {
    "${HOST_PYTHON}" - "${ROOT}" "${SCHEMA}" "${SCRIPT_VERSION}" <<'PY'
import csv
import hashlib
import json
import pathlib
import sys
from datetime import datetime, timezone

root = pathlib.Path(sys.argv[1]).resolve()
schema = sys.argv[2]
script_version = sys.argv[3]
score = root / "score"
with (score / "metrics.tsv").open(newline="") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))
by_scope = {row["scope"]: row for row in rows if row["arm"] == "pancollapse"}
assert set(by_scope) == {"overall", "chr20", "chr21", "chr22"}
overall = by_scope["overall"]
assert int(overall["all_mapped_origin_truth_reads"]) == 1_000_000
assert int(overall["eligible_truth_reads"]) + int(overall["background_truth_reads"]) == 1_000_000
assert sum(int(by_scope[scope]["all_mapped_origin_truth_reads"]) for scope in ("chr20", "chr21", "chr22")) == 1_000_000

score_manifest = json.loads((score / "MANIFEST.json").read_text())
results = score_manifest["results"]
assert results["fastq_reads"] == 1_000_000
assert results["scored_truth_reads"] == 1_000_000
assert results["excluded_unknown_truth"] == 0

def record(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(8 << 20), b""):
            digest.update(block)
    return {"path": str(path.resolve()), "bytes": path.stat().st_size, "sha256": digest.hexdigest()}

payload = {
    "schema": schema,
    "script_version": script_version,
    "status": "complete",
    "evaluation_status": "authorized_partial_unblinding",
    "full_final_test_complete": False,
    "sample": {
        "n": 1_000_000,
        "selection": "lowest_sha256(seed_NUL_read_name)",
        "seed": "pansc-hg002-joint-v090-gene-assignment-1m-v1",
        "assignment_blind": True,
    },
    "barcode_correction": {
        "prior_universe": "all 11881577 producer BAM records",
        "pass2_prefilter": "compiled htslib selected-QNAME filter to uncompressed pipe",
        "output_threads": 8,
        "full_corrected_bam_written": False,
    },
    "metric_contract": {
        "primary": "overall eligibility-aware uniform-truth-gene F1",
        "mandatory_co_report": "overall eligibility-aware read-weighted precision, recall, and F1",
        "diagnostic": "per-chromosome metrics and all-mapped-origin read-weighted and gene-unit metrics",
        "missing_classification": "false negative",
        "matrix_qc_run": False,
    },
    "completed_utc": datetime.now(timezone.utc).isoformat(),
    "outputs": {
        "metrics": record(score / "metrics.tsv"),
        "score_manifest": record(score / "MANIFEST.json"),
        "read_classification": record(root / "count" / "read_classification.tsv.gz"),
    },
}
(root / "BENCHMARK.MANIFEST.json").write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
PY
    printf 'benchmark_complete\treads=%s\tevaluation_status=authorized_partial_unblinding\tfull_final_test_complete=false\tutc=%s\n' \
        "${SAMPLE_SIZE}" "$(timestamp)" > "${RUN_STATUS}"
}

preflight
printf 'Preflight complete: exact frozen inputs and 1,000,000-read subset validated.\n'
if ((PREFLIGHT_ONLY)); then
    exit 0
fi

mkdir -p "${LOG_ROOT}" "${COMMAND_ROOT}" "${TIMING_ROOT}"
if ((RESUME_AFTER_CORRECTION)); then
    for path in \
        "${ROOT}/count.partial" "${ROOT}/count" \
        "${ROOT}/score.partial" "${ROOT}/score" \
        "${ROOT}/BENCHMARK.MANIFEST.json"; do
        [[ ! -e "${path}" ]] || fail "resume refuses existing later-stage artifact: ${path}"
    done
    CURRENT_STAGE="resume_correction"
    resume_completed_correction
else
    for path in \
        "${ROOT}/correct_barcodes.partial" "${ROOT}/correct_barcodes" \
        "${ROOT}/count.partial" "${ROOT}/count" \
        "${ROOT}/score.partial" "${ROOT}/score" \
        "${ROOT}/BENCHMARK.MANIFEST.json"; do
        [[ ! -e "${path}" ]] || fail "refusing existing benchmark artifact: ${path}"
    done
    {
        printf 'schema\t%s\n' "${SCHEMA}"
        printf 'script_version\t%s\n' "${SCRIPT_VERSION}"
        printf 'evaluation_status\tauthorized_partial_unblinding\n'
        printf 'full_final_test_complete\tfalse\n'
        printf 'sample_n\t%s\n' "${SAMPLE_SIZE}"
        printf 'seed\t%s\n' "${SEED}"
        printf 'source_bam\t%s\n' "${SOURCE_BAM}"
        printf 'source_bam_sha256\t%s\n' "${SOURCE_BAM_SHA256}"
        printf 'freeze_root\t%s\n' "${FREEZE_ROOT}"
        printf 'annotation_root\t%s\n' "${ANNOTATION_ROOT}"
        printf 'direct_policy\t%s\n' "${DIRECT_POLICY}"
        printf 'projected_policy\t%s\n' "${PROJECTED_POLICY}"
    } > "${ROOT}/BENCHMARK.CONFIG.tsv"
    printf 'benchmark_running\tstage=correct_barcodes\tutc=%s\n' "$(timestamp)" > "${RUN_STATUS}"
    CURRENT_STAGE="correct_barcodes"
    run_correction
fi
printf 'benchmark_running\tstage=count\tutc=%s\n' "$(timestamp)" > "${RUN_STATUS}"
CURRENT_STAGE="count"
run_count
printf 'benchmark_running\tstage=score\tutc=%s\n' "$(timestamp)" > "${RUN_STATUS}"
CURRENT_STAGE="score"
run_score
CURRENT_STAGE="finalize"
finalize_benchmark
printf 'Benchmark complete: %s\n' "${ROOT}"
