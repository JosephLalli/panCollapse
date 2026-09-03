add_executable(vg_link_smoke vg_link_smoke.cpp)
target_link_libraries(vg_link_smoke PRIVATE panCollapse::vg_boundary panCollapse::warnings)
add_test(NAME vg_link_smoke COMMAND vg_link_smoke)
set_tests_properties(vg_link_smoke PROPERTIES LABELS "vg;link")

# PathTally unit tests (self-contained; no external data).
add_executable(pathtally_score_test pathtally_score_test.cpp)
target_include_directories(pathtally_score_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(pathtally_score_test PRIVATE panCollapse::vg_boundary panCollapse::warnings)
add_test(NAME pathtally_score_test COMMAND pathtally_score_test)
set_tests_properties(pathtally_score_test PROPERTIES LABELS "vg;pathtally")

add_executable(pathtally_pipeline_test pathtally_pipeline_test.cpp)
target_include_directories(pathtally_pipeline_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(pathtally_pipeline_test PRIVATE panCollapse::vg_boundary panCollapse::warnings)
add_test(NAME pathtally_pipeline_test COMMAND pathtally_pipeline_test)
set_tests_properties(pathtally_pipeline_test PROPERTIES LABELS "vg;pathtally")

# Ledger count-mode rules (Gene/GeneFull/variants); pure, no graph.
add_executable(pathtally_ledger_test pathtally_ledger_test.cpp)
target_include_directories(pathtally_ledger_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(pathtally_ledger_test PRIVATE panCollapse::warnings)
add_test(NAME pathtally_ledger_test COMMAND pathtally_ledger_test)
set_tests_properties(pathtally_ledger_test PROPERTIES LABELS "vg;pathtally;ledger")

add_executable(path_identity_ledger_test path_identity_ledger_test.cpp)
target_include_directories(path_identity_ledger_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(path_identity_ledger_test PRIVATE panCollapse::warnings)
add_test(NAME path_identity_ledger_test COMMAND path_identity_ledger_test)
set_tests_properties(path_identity_ledger_test PROPERTIES LABELS "vg;pathtally;identity-ledger")

# Self-contained end-to-end smoke: a tiny hand-authored graph + multipath GAMP go
# through panCollapse, and the independent oracle checks the RAD exactly.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(PANCOLLAPSE_ALEVIN_FRY alevin-fry HINTS "/mnt/ssd/lalli/usr/local/bin"
  DOC "alevin-fry executable for the RAD interoperability test")
if(NOT PANCOLLAPSE_ALEVIN_FRY)
  message(FATAL_ERROR "alevin-fry executable not found; set -DPANCOLLAPSE_ALEVIN_FRY=/path/to/alevin-fry")
endif()
find_program(PANCOLLAPSE_SAMTOOLS samtools DOC "samtools executable for the BAM output tests")
if(NOT PANCOLLAPSE_SAMTOOLS)
  message(FATAL_ERROR "samtools executable not found; set -DPANCOLLAPSE_SAMTOOLS=/path/to/samtools")
endif()
find_program(PANCOLLAPSE_UMI_TOOLS umi_tools DOC "umi_tools executable for the BAM counting test")
if(NOT PANCOLLAPSE_UMI_TOOLS)
  message(FATAL_ERROR "umi_tools executable not found; set -DPANCOLLAPSE_UMI_TOOLS=/path/to/umi_tools")
endif()
# The BAM tag/count checkers read the BAM with pysam. The interpreter find_package(Python3)
# selects is not necessarily the one that has pysam (and umi_tools), so verify it up front and
# fail with an actionable message rather than at test time.
execute_process(
  COMMAND "${Python3_EXECUTABLE}" -c "import pysam"
  RESULT_VARIABLE PANCOLLAPSE_PYSAM_RC OUTPUT_QUIET ERROR_QUIET)
if(NOT PANCOLLAPSE_PYSAM_RC EQUAL 0)
  message(FATAL_ERROR
    "The BAM tests need the 'pysam' module, but '${Python3_EXECUTABLE}' cannot import it. "
    "Install pysam for that interpreter, or pass "
    "-DPython3_EXECUTABLE=/path/to/python-with-pysam (the same interpreter umi_tools uses).")
endif()
set(PT_SMOKE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pathtally_smoke_work")
set(PT_FIX "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/pathtally_smoke")
set(PT_VG "${PANCOLLAPSE_VG_ROOT}/bin/vg")

add_test(NAME pathtally_smoke_setup
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PT_SMOKE_DIR}")
set_tests_properties(pathtally_smoke_setup PROPERTIES LABELS "vg;pathtally"
  FIXTURES_SETUP pathtally_smoke_dir)

add_test(NAME pathtally_smoke_xg
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> convert -g '${PT_FIX}/graph.gfa' -x > '${PT_SMOKE_DIR}/graph.xg'")
set_tests_properties(pathtally_smoke_xg PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_smoke_inputs)

add_test(NAME pathtally_smoke_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${PT_FIX}/reads.gamp.json' > '${PT_SMOKE_DIR}/reads.gamp'")
set_tests_properties(pathtally_smoke_gamp PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_smoke_inputs)

add_test(NAME pathtally_smoke_paths
  COMMAND /bin/sh -c "grep '^P' '${PT_FIX}/graph.gfa' > '${PT_SMOKE_DIR}/paths.gfa'")
set_tests_properties(pathtally_smoke_paths PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_smoke_inputs)

add_test(NAME pathtally_smoke_convert
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/out")
set_tests_properties(pathtally_smoke_convert PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED pathtally_smoke_inputs FIXTURES_SETUP pathtally_smoke_out)

add_test(NAME pathtally_legacy_requires_explicit_adapter
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/implicit_legacy_out")
set_tests_properties(pathtally_legacy_requires_explicit_adapter PROPERTIES
  LABELS "vg;pathtally;identity-ledger;legacy-adapter"
  FIXTURES_REQUIRED pathtally_smoke_inputs WILL_FAIL TRUE)

# Independent oracle: exact RAD match plus an expected emitted-record count (3).
add_test(NAME pathtally_smoke_oracle
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/pathtally_oracle.py"
    "${PT_FIX}/reads.gamp.json" "${PT_SMOKE_DIR}/paths.gfa" "${PT_FIX}/t2g.tsv"
    "${PT_SMOKE_DIR}/out/map.rad" 3)
set_tests_properties(pathtally_smoke_oracle PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED "pathtally_smoke_out;pathtally_smoke_inputs")

# Optional t2g column 3 aliases arbitrary raw graph paths to canonical transcripts. The
# read on node 2 ties both NM_1.1 raw copies and NM_2.2 at the raw-path top score; the
# oracle verifies that collapse happens only after that tie selection, yielding both
# CANON_A and CANON_B (never a doubled CANON_A score that drops CANON_B).
add_test(NAME pathtally_alias_convert
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g_alias.tsv" --out-dir "${PT_SMOKE_DIR}/alias_out")
set_tests_properties(pathtally_alias_convert PROPERTIES LABELS "vg;pathtally;t2g-alias"
  FIXTURES_REQUIRED pathtally_smoke_inputs FIXTURES_SETUP pathtally_alias_out)

add_test(NAME pathtally_alias_oracle
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/pathtally_oracle.py"
    "${PT_FIX}/reads.gamp.json" "${PT_SMOKE_DIR}/paths.gfa" "${PT_FIX}/t2g_alias.tsv"
    "${PT_SMOKE_DIR}/alias_out/map.rad" 3)
set_tests_properties(pathtally_alias_oracle PROPERTIES LABELS "vg;pathtally;t2g-alias"
  FIXTURES_REQUIRED "pathtally_alias_out;pathtally_smoke_inputs")

add_test(NAME pathtally_alias_tx2gene
  COMMAND /bin/sh -c "printf 'CANON_A\\tGENE1\\nCANON_B\\tGENE2\\n' > '${PT_SMOKE_DIR}/alias.expected.tsv' \
    && cmp '${PT_SMOKE_DIR}/alias.expected.tsv' '${PT_SMOKE_DIR}/alias_out/tx2gene.tsv'")
set_tests_properties(pathtally_alias_tx2gene PROPERTIES LABELS "vg;pathtally;t2g-alias"
  FIXTURES_REQUIRED pathtally_alias_out)

# Alias maps are functional mappings in both directions relevant to counting: one raw
# graph path cannot name conflicting canonical transcripts, and one canonical transcript
# cannot project to multiple genes.
add_test(NAME pathtally_alias_path_conflict
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g_path_conflict.tsv" --out-dir "${PT_SMOKE_DIR}/alias_path_conflict")
set_tests_properties(pathtally_alias_path_conflict PROPERTIES LABELS "vg;pathtally;t2g-alias"
  FIXTURES_REQUIRED pathtally_smoke_inputs WILL_FAIL TRUE)

add_test(NAME pathtally_alias_gene_conflict
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g_gene_conflict.tsv" --out-dir "${PT_SMOKE_DIR}/alias_gene_conflict")
set_tests_properties(pathtally_alias_gene_conflict PROPERTIES LABELS "vg;pathtally;t2g-alias"
  FIXTURES_REQUIRED pathtally_smoke_inputs WILL_FAIL TRUE)

# Optional BAM output. Same convert pass emits reads.bam alongside map.rad; the RAD must be
# byte-identical to the no-BAM smoke run, the BAM must be valid, and every record's tags and
# GX gene set must match the independent oracle.
add_test(NAME pathtally_bam_convert
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/out_bam"
    --bam-out "${PT_SMOKE_DIR}/out_bam/reads.bam")
set_tests_properties(pathtally_bam_convert PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED pathtally_smoke_inputs FIXTURES_SETUP pathtally_bam_out)

add_test(NAME pathtally_bam_quickcheck
  COMMAND "${PANCOLLAPSE_SAMTOOLS}" quickcheck "${PT_SMOKE_DIR}/out_bam/reads.bam")
set_tests_properties(pathtally_bam_quickcheck PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED pathtally_bam_out)

add_test(NAME pathtally_bam_header
  COMMAND /bin/sh -c "'${PANCOLLAPSE_SAMTOOLS}' view -H '${PT_SMOKE_DIR}/out_bam/reads.bam' > '${PT_SMOKE_DIR}/bam_header.txt' \
    && grep -q '^@HD.*SO:unsorted' '${PT_SMOKE_DIR}/bam_header.txt' \
    && grep -q '^@SQ.*SN:GENE1' '${PT_SMOKE_DIR}/bam_header.txt' \
    && grep -q '^@SQ.*SN:GENE2' '${PT_SMOKE_DIR}/bam_header.txt' \
    && grep -q '^@PG.*ID:panCollapse' '${PT_SMOKE_DIR}/bam_header.txt' \
    && ! grep -q 'panCollapse-evidence-schema:panCollapse-superset-v1' '${PT_SMOKE_DIR}/bam_header.txt'")
set_tests_properties(pathtally_bam_header PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED pathtally_bam_out)

# RAD regression: the map.rad written with --bam-out must equal the no-BAM smoke run's.
add_test(NAME pathtally_bam_rad_identical
  COMMAND ${CMAKE_COMMAND} -E compare_files "${PT_SMOKE_DIR}/out_bam/map.rad" "${PT_SMOKE_DIR}/out/map.rad")
set_tests_properties(pathtally_bam_rad_identical PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED "pathtally_bam_out;pathtally_smoke_out")

# Score mode takes the legacy node_hst_cache path (separate from exact Ex50's node_ledger_cache).
# Hold output options stable, force chunk merging, and snapshot default/1/8 worker results before
# byte-comparing every persisted artifact that score mode emits.
set(PT_SCORE_THREADS_DIR "${CMAKE_CURRENT_BINARY_DIR}/pathtally_score_thread_determinism_work")
add_test(NAME pathtally_score_thread_determinism_setup
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PT_SCORE_THREADS_DIR}")
set_tests_properties(pathtally_score_thread_determinism_setup PROPERTIES
  LABELS "vg;pathtally;score;threads"
  FIXTURES_REQUIRED pathtally_smoke_inputs
  FIXTURES_SETUP pathtally_score_thread_determinism_dir)

add_test(NAME pathtally_score_thread_determinism
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/score_thread_determinism.sh"
    "$<TARGET_FILE:panCollapse>" "${PT_SMOKE_DIR}/reads.gamp" "${PT_SMOKE_DIR}/graph.xg"
    "${PT_FIX}/t2g.tsv" "${PT_SCORE_THREADS_DIR}")
set_tests_properties(pathtally_score_thread_determinism PROPERTIES
  LABELS "vg;pathtally;score;threads;multichunk;bam"
  ENVIRONMENT "PANCOLLAPSE_MAX_CHUNK_BYTES=1"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_score_thread_determinism_dir")

add_test(NAME pathtally_bam_parity
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/pathtally_bam_verify.py"
    "${PT_SMOKE_DIR}/out_bam/reads.bam" "${PT_FIX}/reads.gamp.json" "${PT_SMOKE_DIR}/paths.gfa"
    "${PT_FIX}/t2g.tsv" 3 omit)
set_tests_properties(pathtally_bam_parity PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED "pathtally_bam_out;pathtally_smoke_inputs")

# umi_tools counting: convert the multi-UMI af fixture to BAM, sort + index, and count per
# gene per cell. bam_count_verify recomputes the counts from the BAM tags independently, so
# it confirms umi_tools reads the panCollapse tags correctly (expected GENE1=2, GENE2=1).
add_test(NAME pathtally_bam_count_convert
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads_af.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/bam_count"
    --bam-out "${PT_SMOKE_DIR}/bam_count/reads.bam")
set_tests_properties(pathtally_bam_count_convert PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_af_inputs" FIXTURES_SETUP pathtally_bam_count_bam)

add_test(NAME pathtally_bam_count_run
  COMMAND /bin/sh -c "'${PANCOLLAPSE_SAMTOOLS}' sort -o '${PT_SMOKE_DIR}/bam_count/reads.sorted.bam' '${PT_SMOKE_DIR}/bam_count/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' index '${PT_SMOKE_DIR}/bam_count/reads.sorted.bam' \
    && '${PANCOLLAPSE_UMI_TOOLS}' count --per-gene --gene-tag=XT --per-cell --extract-umi-method=tag --umi-tag=UB --cell-tag=CB --method=unique -I '${PT_SMOKE_DIR}/bam_count/reads.sorted.bam' -S '${PT_SMOKE_DIR}/bam_count/counts.tsv'")
set_tests_properties(pathtally_bam_count_run PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED pathtally_bam_count_bam FIXTURES_SETUP pathtally_bam_count_tsv)

add_test(NAME pathtally_bam_count_verify
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/bam_count_verify.py"
    "${PT_SMOKE_DIR}/bam_count/reads.sorted.bam" "${PT_SMOKE_DIR}/bam_count/counts.tsv")
set_tests_properties(pathtally_bam_count_verify PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED pathtally_bam_count_tsv)

# Position independence: two records with identical CB/UMI/XT at different positions must
# count as one molecule. bam_count_verify recomputes ignoring position (naive = 1); umi_tools
# must agree, which it can only do by grouping on the gene tag rather than coordinate.
add_test(NAME pathtally_bam_posindep
  COMMAND /bin/sh -c "'${PANCOLLAPSE_SAMTOOLS}' sort -o '${PT_SMOKE_DIR}/posindep.sorted.bam' '${PT_FIX}/position_independence.sam' \
    && '${PANCOLLAPSE_SAMTOOLS}' index '${PT_SMOKE_DIR}/posindep.sorted.bam' \
    && '${PANCOLLAPSE_UMI_TOOLS}' count --per-gene --gene-tag=XT --per-cell --extract-umi-method=tag --umi-tag=UB --cell-tag=CB --method=unique -I '${PT_SMOKE_DIR}/posindep.sorted.bam' -S '${PT_SMOKE_DIR}/posindep.counts.tsv'")
set_tests_properties(pathtally_bam_posindep PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_bam_posindep_tsv)

add_test(NAME pathtally_bam_posindep_verify
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/bam_count_verify.py"
    "${PT_SMOKE_DIR}/posindep.sorted.bam" "${PT_SMOKE_DIR}/posindep.counts.tsv")
set_tests_properties(pathtally_bam_posindep_verify PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED pathtally_bam_posindep_tsv)

# Orientation filter: in the smoke fixture readfwd/readmulti are forward, readrev is reverse.
# --strand keeps only matching FEATURE records and counts the drops in strand_filtered_groups;
# every valid group still has one BAM record, with filtered/no-compatible groups barcode-only.
add_test(NAME pathtally_strand_forward
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${PT_SMOKE_DIR}/reads.gamp' --xg '${PT_SMOKE_DIR}/graph.xg' --legacy-adapter hst-v1 --t2g '${PT_FIX}/t2g.tsv' \
    --strand forward --out-dir '${PT_SMOKE_DIR}/strand_fwd' --bam-out '${PT_SMOKE_DIR}/strand_fwd/reads.bam' \
    && grep -qP '^emitted_groups\\t2$' '${PT_SMOKE_DIR}/strand_fwd/summary.tsv' \
    && grep -qP '^strand_filtered_groups\\t1$' '${PT_SMOKE_DIR}/strand_fwd/summary.tsv' \
    && grep -qP '^bam_records\\t4$' '${PT_SMOKE_DIR}/strand_fwd/summary.tsv' \
    && grep -qP '^barcode_only_bam_records\\t2$' '${PT_SMOKE_DIR}/strand_fwd/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${PT_SMOKE_DIR}/strand_fwd/reads.bam' \
       | grep readrev | grep -q 'XB:Z:barcode_only'")
set_tests_properties(pathtally_strand_forward PROPERTIES LABELS "vg;pathtally;strand"
  FIXTURES_REQUIRED pathtally_smoke_inputs)

add_test(NAME pathtally_strand_reverse
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${PT_SMOKE_DIR}/reads.gamp' --xg '${PT_SMOKE_DIR}/graph.xg' --legacy-adapter hst-v1 --t2g '${PT_FIX}/t2g.tsv' \
    --strand reverse --out-dir '${PT_SMOKE_DIR}/strand_rev' --bam-out '${PT_SMOKE_DIR}/strand_rev/reads.bam' \
    && grep -qP '^emitted_groups\\t1$' '${PT_SMOKE_DIR}/strand_rev/summary.tsv' \
    && grep -qP '^strand_filtered_groups\\t2$' '${PT_SMOKE_DIR}/strand_rev/summary.tsv' \
    && grep -qP '^bam_records\\t4$' '${PT_SMOKE_DIR}/strand_rev/summary.tsv' \
    && grep -qP '^barcode_only_bam_records\\t3$' '${PT_SMOKE_DIR}/strand_rev/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${PT_SMOKE_DIR}/strand_rev/reads.bam' | grep -q readrev")
set_tests_properties(pathtally_strand_reverse PROPERTIES LABELS "vg;pathtally;strand"
  FIXTURES_REQUIRED pathtally_smoke_inputs)

# Multi-chunk path: force one chunk per record and confirm the oracle reads all
# chunks and still matches exactly.
add_test(NAME pathtally_smoke_convert_multichunk
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/out_mc")
set_tests_properties(pathtally_smoke_convert_multichunk PROPERTIES LABELS "vg;pathtally"
  ENVIRONMENT "PANCOLLAPSE_MAX_CHUNK_BYTES=1"
  FIXTURES_REQUIRED pathtally_smoke_inputs FIXTURES_SETUP pathtally_smoke_mc_out)

add_test(NAME pathtally_smoke_oracle_multichunk
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/pathtally_oracle.py"
    "${PT_FIX}/reads.gamp.json" "${PT_SMOKE_DIR}/paths.gfa" "${PT_FIX}/t2g.tsv"
    "${PT_SMOKE_DIR}/out_mc/map.rad" 3)
set_tests_properties(pathtally_smoke_oracle_multichunk PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED "pathtally_smoke_mc_out;pathtally_smoke_inputs")

# Hermetic alevin-fry interoperability: PathTally RAD -> generate-permit-list ->
# collate -> quant, asserting the exact cell-by-gene matrix (GENE1=2, GENE2=1).
add_test(NAME pathtally_af_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${PT_FIX}/reads_af.gamp.json' > '${PT_SMOKE_DIR}/reads_af.gamp'")
set_tests_properties(pathtally_af_gamp PROPERTIES LABELS "vg;pathtally;interop"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_af_inputs)

add_test(NAME pathtally_af_convert
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads_af.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/af_out")
set_tests_properties(pathtally_af_convert PROPERTIES LABELS "vg;pathtally;interop"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_af_inputs" FIXTURES_SETUP pathtally_af_rad)

add_test(NAME pathtally_af_permit
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" generate-permit-list -i "${PT_SMOKE_DIR}/af_out" -d fw
    -o "${PT_SMOKE_DIR}/af_pl" -f 1 -t 1)
set_tests_properties(pathtally_af_permit PROPERTIES LABELS "vg;pathtally;interop"
  FIXTURES_REQUIRED pathtally_af_rad FIXTURES_SETUP pathtally_af_permit)

add_test(NAME pathtally_af_collate
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" collate -i "${PT_SMOKE_DIR}/af_pl" -r "${PT_SMOKE_DIR}/af_out" -t 1)
set_tests_properties(pathtally_af_collate PROPERTIES LABELS "vg;pathtally;interop"
  FIXTURES_REQUIRED pathtally_af_permit FIXTURES_SETUP pathtally_af_collated)

add_test(NAME pathtally_af_quant
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" quant -i "${PT_SMOKE_DIR}/af_pl" -m "${PT_SMOKE_DIR}/af_out/tx2gene.tsv"
    -o "${PT_SMOKE_DIR}/af_quant" -r cr-like -t 1 --use-mtx)
set_tests_properties(pathtally_af_quant PROPERTIES LABELS "vg;pathtally;interop"
  FIXTURES_REQUIRED pathtally_af_collated FIXTURES_SETUP pathtally_af_quant)

add_test(NAME pathtally_af_verify
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/pathtally_af_verify.py"
    "${PT_SMOKE_DIR}/af_quant/alevin")
set_tests_properties(pathtally_af_verify PROPERTIES LABELS "vg;pathtally;interop"
  FIXTURES_REQUIRED pathtally_af_quant)

# Scoring fidelity: simulate reads from the HST paths, map them with mpmap so the
# GAMP carries vg's own Subpath.score, and confirm the per-node scorer reproduces
# those scores. Covers both the flat and quality-adjusted scorers.
add_test(NAME pathtally_fid_build
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/build_fidelity_gamp.sh"
    "${PT_VG}" "${PT_FIX}/graph.gfa" "${PT_SMOKE_DIR}/graph.xg" "${PT_SMOKE_DIR}")
set_tests_properties(pathtally_fid_build PROPERTIES LABELS "vg;pathtally;fidelity"
  FIXTURES_REQUIRED pathtally_smoke_inputs FIXTURES_SETUP pathtally_fid)

add_test(NAME pathtally_fid_flat
  COMMAND pathtally_score_test "${PT_SMOKE_DIR}/fid.gamp")
set_tests_properties(pathtally_fid_flat PROPERTIES LABELS "vg;pathtally;fidelity"
  FIXTURES_REQUIRED pathtally_fid)

add_test(NAME pathtally_fid_qualadj
  COMMAND pathtally_score_test "${PT_SMOKE_DIR}/fid.gamp" "${PT_SMOKE_DIR}/graph.xg")
set_tests_properties(pathtally_fid_qualadj PROPERTIES LABELS "vg;pathtally;fidelity"
  FIXTURES_REQUIRED pathtally_fid)

# Streaming stdin must produce a byte-identical RAD to the file-input smoke run.
add_test(NAME pathtally_smoke_stdin
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert --gamp - \
    --xg '${PT_SMOKE_DIR}/graph.xg' --legacy-adapter hst-v1 --t2g '${PT_FIX}/t2g.tsv' \
    --out-dir '${PT_SMOKE_DIR}/out_stdin' < '${PT_SMOKE_DIR}/reads.gamp' \
    && cmp '${PT_SMOKE_DIR}/out_stdin/map.rad' '${PT_SMOKE_DIR}/out/map.rad'")
set_tests_properties(pathtally_smoke_stdin PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_smoke_out")

# Molecule-identity policy: a malformed CB/UMI name is skipped by default (and the
# summary counts it), but the fail policy aborts the run.
add_test(NAME pathtally_badmol_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${PT_FIX}/reads_badmol.gamp.json' > '${PT_SMOKE_DIR}/reads_badmol.gamp'")
set_tests_properties(pathtally_badmol_gamp PROPERTIES LABELS "vg;pathtally;molecule"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_badmol_inputs)

add_test(NAME pathtally_badmol_skip
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${PT_SMOKE_DIR}/reads_badmol.gamp' --xg '${PT_SMOKE_DIR}/graph.xg' \
    --legacy-adapter hst-v1 --t2g '${PT_FIX}/t2g.tsv' --out-dir '${PT_SMOKE_DIR}/badmol_out' \
    && grep -qP '^raw_molecule_malformed_groups\\t1$' '${PT_SMOKE_DIR}/badmol_out/summary.tsv' \
    && grep -qP '^raw_molecule_skipped_groups\\t1$' '${PT_SMOKE_DIR}/badmol_out/summary.tsv' \
    && grep -qP '^emitted_groups\\t0$' '${PT_SMOKE_DIR}/badmol_out/summary.tsv'")
set_tests_properties(pathtally_badmol_skip PROPERTIES LABELS "vg;pathtally;molecule"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_badmol_inputs")

add_test(NAME pathtally_badmol_fail
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads_badmol.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/badmol_fail_out"
    --molecule-identity-failures fail)
set_tests_properties(pathtally_badmol_fail PROPERTIES LABELS "vg;pathtally;molecule"
  WILL_FAIL TRUE
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_badmol_inputs")

# A completed read name that recurs later is a grouping violation and must abort.
add_test(NAME pathtally_recur_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${PT_FIX}/reads_recur.gamp.json' > '${PT_SMOKE_DIR}/reads_recur.gamp'")
set_tests_properties(pathtally_recur_gamp PROPERTIES LABELS "vg;pathtally;grouping"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_recur_inputs)

add_test(NAME pathtally_recur_fail
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads_recur.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/recur_out")
set_tests_properties(pathtally_recur_fail PROPERTIES LABELS "vg;pathtally;grouping"
  WILL_FAIL TRUE
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_recur_inputs")

# A failed run must not leave a valid-looking map.rad (N10: atomic temp-rename).
add_test(NAME pathtally_recur_no_partial
  COMMAND /bin/sh -c "! '$<TARGET_FILE:panCollapse>' convert \
    --gamp '${PT_SMOKE_DIR}/reads_recur.gamp' --xg '${PT_SMOKE_DIR}/graph.xg' \
    --legacy-adapter hst-v1 --t2g '${PT_FIX}/t2g.tsv' --out-dir '${PT_SMOKE_DIR}/recur_no_partial_out' \
    && test ! -f '${PT_SMOKE_DIR}/recur_no_partial_out/map.rad'")
set_tests_properties(pathtally_recur_no_partial PROPERTIES LABELS "vg;pathtally;grouping"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_recur_inputs")

# Recurrence detection must remain fail-closed after records are dispatched to workers.
add_test(NAME pathtally_recur_threaded_no_partial
  COMMAND /bin/sh -c "! '$<TARGET_FILE:panCollapse>' convert \
    --gamp '${PT_SMOKE_DIR}/reads_recur.gamp' --xg '${PT_SMOKE_DIR}/graph.xg' \
    --legacy-adapter hst-v1 --t2g '${PT_FIX}/t2g.tsv' --threads 8 \
    --out-dir '${PT_SMOKE_DIR}/recur_threaded_no_partial_out' \
    && test ! -f '${PT_SMOKE_DIR}/recur_threaded_no_partial_out/map.rad'")
set_tests_properties(pathtally_recur_threaded_no_partial PROPERTIES
  LABELS "vg;pathtally;grouping;threads"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_recur_inputs")

# A GAMP aligned against a different graph (node IDs absent from the xg) must fail loudly.
add_test(NAME pathtally_badnode_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${PT_FIX}/reads_badnode.gamp.json' > '${PT_SMOKE_DIR}/reads_badnode.gamp'")
set_tests_properties(pathtally_badnode_gamp PROPERTIES LABELS "vg;pathtally;node-space"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_badnode_inputs)

add_test(NAME pathtally_badnode_fail
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads_badnode.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --legacy-adapter hst-v1 --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/badnode_out")
set_tests_properties(pathtally_badnode_fail PROPERTIES LABELS "vg;pathtally;node-space"
  WILL_FAIL TRUE
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_badnode_inputs")

# GeneFull gene-calling needs no panCollapse mode flag: it is spliced counting run against a
# graph annotated with gene-body paths and selected by the t2g. scripts/make-gene-annotation.sh
# embeds one unspliced gene-body path per gene (covering introns) and writes the gene t2g. The
# fixture gene GENE1 has exon nodes 1,3 (HST NM_1.1_H1 = 1+,3+) and intron node 2 (on no HST);
# exonread hits nodes 1,3, intronread hits node 2 only. The SAME annotated graph gives a spliced
# count with the HST t2g and a GeneFull count with the gene t2g.
set(GF_DIR "${CMAKE_CURRENT_BINARY_DIR}/genefull_work")
set(GF_FIX "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/genefull_smoke")

add_test(NAME genefull_setup COMMAND ${CMAKE_COMMAND} -E make_directory "${GF_DIR}")
set_tests_properties(genefull_setup PROPERTIES LABELS "vg;pathtally;genefull"
  FIXTURES_SETUP genefull_dir)

add_test(NAME genefull_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${GF_FIX}/reads.gamp.json' > '${GF_DIR}/reads.gamp'")
set_tests_properties(genefull_gamp PROPERTIES LABELS "vg;pathtally;genefull"
  FIXTURES_REQUIRED genefull_dir FIXTURES_SETUP genefull_reads)

# Annotate: embed gene-body paths and write the gene t2g from the GTF + graph.
add_test(NAME genefull_annotate
  COMMAND /bin/sh -c "'${CMAKE_SOURCE_DIR}/scripts/make-gene-annotation.sh' \
    -x '${GF_FIX}/graph.gfa' -n '${GF_FIX}/annotation.gtf' -o '${GF_DIR}/genefull' --vg '${PT_VG}'")
set_tests_properties(genefull_annotate PROPERTIES LABELS "vg;pathtally;genefull"
  FIXTURES_REQUIRED genefull_dir FIXTURES_SETUP genefull_annotated)

# Spliced count: annotated graph + HST t2g. The intronic read has no count feature (1 RAD emitted,
# 1 no-compat), but its raw CB/UMI/CY/UY remains in the optional BAM as one unmapped
# XB:Z:barcode_only record so downstream correction can use the all-read barcode prior.
add_test(NAME genefull_spliced_drops_intron
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g.tsv' \
    --out-dir '${GF_DIR}/spliced' --bam-out '${GF_DIR}/spliced/reads.bam' \
    && grep -qP '^emitted_groups\\t1$' '${GF_DIR}/spliced/summary.tsv' \
    && grep -qP '^no_compatible_transcript_groups\\t1$' '${GF_DIR}/spliced/summary.tsv' \
    && grep -qP '^bam_records\\t2$' '${GF_DIR}/spliced/summary.tsv' \
    && grep -qP '^barcode_only_bam_records\\t1$' '${GF_DIR}/spliced/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/spliced/reads.bam' \
       | grep -w exonread | grep 'CY:Z:IIIIIIIIIIIIIIII' | grep -q 'UY:Z:JJJJJJJJJJJJ' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/spliced/reads.bam' \
       | grep -w intronread | grep 'XB:Z:barcode_only' | grep 'CY:Z:::::::::::::::::' \
       | grep -q 'UY:Z:KKKKKKKKKKKK' \
    && ! ('${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/spliced/reads.bam' \
       | grep -w intronread | grep -q 'GX:Z:')")
set_tests_properties(genefull_spliced_drops_intron PROPERTIES LABELS "vg;pathtally;genefull"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

# GeneFull count: the SAME annotated graph + the generated gene t2g. The intronic read is now
# called to its gene (2 emitted, 0 no-compat), carried in the BAM as GENE1. No mode flag.
add_test(NAME genefull_full_calls_intron
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' --legacy-adapter hst-v1 --t2g '${GF_DIR}/genefull.t2g.tsv' \
    --out-dir '${GF_DIR}/full' --bam-out '${GF_DIR}/full/reads.bam' \
    && grep -qP '^emitted_groups\\t2$' '${GF_DIR}/full/summary.tsv' \
    && grep -qP '^no_compatible_transcript_groups\\t0$' '${GF_DIR}/full/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' quickcheck '${GF_DIR}/full/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/full/reads.bam' | grep intronread | grep -q 'GX:Z:GENE1'")
set_tests_properties(genefull_full_calls_intron PROPERTIES LABELS "vg;pathtally;genefull;bam"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

# Ledger modes emit a per-TRANSCRIPT ledger, never a gene-level summary: the count mode and gene
# grouping/ambiguity are applied downstream by count_cr, so panCollapse tags each read's compatible
# transcripts with TX (transcript ids), GX/GD (gene, orientation -- parallel to TX, one entry per
# emitted transcript), and GL (S=spliced/exon-only, U=unspliced/intron-touching). On the
# both-annotated graph (exon HST t2g + gene-body t2g) the single-isoform gene's only transcript
# (NM_1.1) explains the exonic read exon-only (TX:Z:NM_1.1 GL:Z:S) and spans-but-does-not-explain the
# intronic read (TX:Z:NM_1.1 GL:Z:U) -- count_cr gene mode keeps the former (spliced-only) and drops
# the latter. Both are single-gene, so both reach the RAD here (2 emitted, 0 no-compat). (Rule
# semantics: pathtally_ledger_test.)
add_test(NAME genefull_ledger_gene
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode gene --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g.tsv' --body-t2g '${GF_DIR}/genefull.t2g.tsv' \
    --out-dir '${GF_DIR}/ledger_gene' --bam-out '${GF_DIR}/ledger_gene/reads.bam' \
    && grep -qP '^emitted_groups\\t2$' '${GF_DIR}/ledger_gene/summary.tsv' \
    && grep -qP '^no_compatible_transcript_groups\\t0$' '${GF_DIR}/ledger_gene/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_gene/reads.bam' | grep exonread | grep -q 'TX:Z:NM_1.1' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_gene/reads.bam' | grep exonread | grep -q 'GL:Z:S' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_gene/reads.bam' | grep intronread | grep -q 'TX:Z:NM_1.1' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_gene/reads.bam' | grep intronread | grep -q 'GL:Z:U'")
set_tests_properties(genefull_ledger_gene PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

add_test(NAME genefull_ledger_genefull
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g.tsv' --body-t2g '${GF_DIR}/genefull.t2g.tsv' \
    --out-dir '${GF_DIR}/ledger_full' \
    && grep -qP '^emitted_groups\\t2$' '${GF_DIR}/ledger_full/summary.tsv'")
set_tests_properties(genefull_ledger_genefull PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

# A bare, suffix-stripped tx<TAB>gene (the standard downstream format) must give the SAME ledger
# result as the path-keyed t2g -- panCollapse strips each embedded path's _R<n>/_H<n> copy suffix and
# falls back to the bare id. Regression guard for the v0.4.1 bare-t2g fallback.
add_test(NAME genefull_ledger_bare_t2g
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/bare_t2g_regression.sh" "$<TARGET_FILE:panCollapse>" "${GF_FIX}" "${GF_DIR}")
set_tests_properties(genefull_ledger_bare_t2g PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

# Ledger mode uses the same optional path aliases. Two raw exon paths for the same CAT
# transcript collapse to one canonical target; their path scores are max-collapsed, not
# summed, before transcript classification.
add_test(NAME genefull_ledger_alias_t2g
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g_alias.tsv' --body-t2g '${GF_DIR}/genefull.t2g.tsv' \
    --out-dir '${GF_DIR}/ledger_alias' --bam-out '${GF_DIR}/ledger_alias/reads.bam' \
    && grep -qP '^emitted_groups\\t2$' '${GF_DIR}/ledger_alias/summary.tsv' \
    && grep -qP '^emitted_target_count_histogram\\t1:2$' '${GF_DIR}/ledger_alias/summary.tsv' \
    && grep -qP '^ENST_CANONICAL\\tGENE1$' '${GF_DIR}/ledger_alias/tx2gene.tsv' \
    && test \"$('${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_alias/reads.bam' | grep -c 'TX:Z:ENST_CANONICAL')\" -eq 2")
set_tests_properties(genefull_ledger_alias_t2g PROPERTIES LABELS "vg;pathtally;genefull;ledger;t2g-alias"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

# Three-column aliases are exact raw graph-path mappings. Naming only a nonexistent raw path whose
# canonical target happens to equal the suffix-stripped NM_1.1_H1 graph path must NOT enable the
# legacy bare-t2g fallback. With no listed exon path there are no transcript spans/calls here.
add_test(NAME genefull_ledger_alias_exact_only
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g_alias_exact_only.tsv' \
    --body-t2g '${GF_DIR}/genefull.t2g.tsv' --out-dir '${GF_DIR}/ledger_alias_exact_only' \
    && grep -qP '^no_compatible_transcript_groups\\t2$' \
       '${GF_DIR}/ledger_alias_exact_only/summary.tsv' \
    && grep -qP '^emitted_groups\\t0$' '${GF_DIR}/ledger_alias_exact_only/summary.tsv'")
set_tests_properties(genefull_ledger_alias_exact_only PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;t2g-alias"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

# Adversarial MAX-vs-sum guard for ledger aliases. This read covers body nodes 1,2,3:
# the body path scores 40, while each of the two identical raw exon paths scores 30.
# MAX collapse therefore emits the canonical transcript as U. Summing the raw exon copies
# would manufacture score 60 and incorrectly emit S instead.
add_test(NAME genefull_alias_max_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${GF_FIX}/reads_alias_max.gamp.json' > '${GF_DIR}/alias_max.gamp'")
set_tests_properties(genefull_alias_max_gamp PROPERTIES LABELS "vg;pathtally;genefull;ledger;t2g-alias"
  FIXTURES_REQUIRED genefull_dir FIXTURES_SETUP genefull_alias_max_reads)

add_test(NAME genefull_ledger_alias_max_not_sum
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/alias_max.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g_alias.tsv' --body-t2g '${GF_DIR}/genefull.t2g.tsv' \
    --out-dir '${GF_DIR}/ledger_alias_max' --bam-out '${GF_DIR}/ledger_alias_max/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_alias_max/reads.bam' \
       | grep 'TX:Z:ENST_CANONICAL' | grep -q 'GL:Z:U'")
set_tests_properties(genefull_ledger_alias_max_not_sum PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;t2g-alias"
  FIXTURES_REQUIRED "genefull_annotated;genefull_alias_max_reads")

# D063 transcript-body t2g: both the vg-rna gene-body path and a second raw copy map to the
# SAME canonical transcript. This file drives the transcript-specific classifier; the legacy
# generated two-column file above continues to drive the untouched D060 tests.
add_test(NAME genefull_transcript_body_t2g
  COMMAND /bin/sh -c "awk -F'\\t' 'BEGIN { OFS=\"\\t\" } { print $1, $2, \"ENST_CANONICAL\" }' \
      '${GF_DIR}/genefull.t2g.tsv' > '${GF_DIR}/transcript_body.t2g.tsv' \
    && printf 'CAT_BODY_COPY\\tGENE1\\tENST_CANONICAL\\n' >> '${GF_DIR}/transcript_body.t2g.tsv'")
set_tests_properties(genefull_transcript_body_t2g PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;transcript-body"
  FIXTURES_REQUIRED genefull_annotated FIXTURES_SETUP genefull_transcript_body_t2g_ready)

add_test(NAME genefull_ledger_transcript_body_calls
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g_alias.tsv' \
    --body-t2g '${GF_DIR}/transcript_body.t2g.tsv' --bam-multigene all \
    --out-dir '${GF_DIR}/ledger_transcript_body' --bam-out '${GF_DIR}/ledger_transcript_body/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_transcript_body/reads.bam' \
       | grep exonread | grep 'TX:Z:ENST_CANONICAL' | grep -q 'GL:Z:S' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_transcript_body/reads.bam' \
       | grep intronread | grep 'TX:Z:ENST_CANONICAL' | grep -q 'GL:Z:U'")
set_tests_properties(genefull_ledger_transcript_body_calls PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;transcript-body"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads;genefull_transcript_body_t2g_ready")

# Production identity contract: exact headered ledger input, exact XG path/length binding,
# path -> Parent -> canonical MAX, literal suffix identities, and lossless tied-winner BAM tags.
add_test(NAME genefull_identity_score
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --path-identity-ledger '${GF_FIX}/path_identity_ledger.tsv' --bam-multigene all \
    --out-dir '${GF_DIR}/identity_score' --bam-out '${GF_DIR}/identity_score/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/identity_score/reads.bam' > '${GF_DIR}/identity_score/read.sam' \
    && grep exonread '${GF_DIR}/identity_score/read.sam' | grep -Fq 'TX:Z:ENST_CANONICAL;literal_R1' \
    && grep exonread '${GF_DIR}/identity_score/read.sam' | grep -Fq 'XP:Z:CAT_TX_COPY,NM_1.1_H1,TX_BODY_COPY2;TX_BODY_COPY1' \
    && grep exonread '${GF_DIR}/identity_score/read.sam' | grep -Fq 'XU:Z:UP_REF,panSCup1_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef;literal_R1' \
    && grep exonread '${GF_DIR}/identity_score/read.sam' | grep -Pq '\tGX:Z:GENE1\t' \
    && grep exonread '${GF_DIR}/identity_score/read.sam' | grep -Pq '\tGD:Z:F\t'")
set_tests_properties(genefull_identity_score PROPERTIES
  LABELS "vg;pathtally;identity-ledger;bam"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

add_test(NAME genefull_identity_count
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --path-identity-ledger '${GF_FIX}/path_identity_ledger.tsv' --count-mode genefull \
    --bam-multigene all --out-dir '${GF_DIR}/identity_count' \
    --bam-out '${GF_DIR}/identity_count/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/identity_count/reads.bam' > '${GF_DIR}/identity_count/read.sam' \
    && grep exonread '${GF_DIR}/identity_count/read.sam' | grep -Fq 'TX:Z:ENST_CANONICAL;literal_R1' \
    && grep exonread '${GF_DIR}/identity_count/read.sam' | grep -Fq 'XP:Z:CAT_TX_COPY,NM_1.1_H1,TX_BODY_COPY2;TX_BODY_COPY1' \
    && grep exonread '${GF_DIR}/identity_count/read.sam' | grep -Fq 'XU:Z:UP_REF,panSCup1_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef;literal_R1' \
    && grep exonread '${GF_DIR}/identity_count/read.sam' | grep -Fq 'GX:Z:GENE1;GENE1' \
    && grep intronread '${GF_DIR}/identity_count/read.sam' | grep -Fq 'XP:Z:CAT_BODY_COPY,GENE1_R1,REF_BODY;TX_FULL_EXON' \
    && grep intronread '${GF_DIR}/identity_count/read.sam' | grep -Fq 'XU:Z:BODY_UP_REF,panSCbody1_fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210;BODY_LITERAL'")
set_tests_properties(genefull_identity_count PROPERTIES
  LABELS "vg;pathtally;identity-ledger;ledger;bam"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

add_test(NAME genefull_identity_count_requires_complete_body_links
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${GF_DIR}/reads.gamp" --xg "${GF_DIR}/genefull.xg"
    --path-identity-ledger "${GF_FIX}/path_identity_ledger_incomplete_body.tsv"
    --count-mode genefull --bam-multigene all
    --out-dir "${GF_DIR}/identity_incomplete_body"
    --bam-out "${GF_DIR}/identity_incomplete_body/reads.bam")
set_tests_properties(genefull_identity_count_requires_complete_body_links PROPERTIES
  LABELS "vg;pathtally;identity-ledger;ledger;bam;ex50"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads" WILL_FAIL TRUE)

add_test(NAME genefull_identity_missing_xg_path
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${GF_DIR}/reads.gamp" --xg "${GF_DIR}/genefull.xg"
    --path-identity-ledger "${GF_FIX}/path_identity_ledger_missing_graph.tsv"
    --out-dir "${GF_DIR}/identity_missing_path")
set_tests_properties(genefull_identity_missing_xg_path PROPERTIES
  LABELS "vg;pathtally;identity-ledger" FIXTURES_REQUIRED "genefull_annotated;genefull_reads"
  WILL_FAIL TRUE)

add_test(NAME genefull_identity_xg_length_mismatch
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${GF_DIR}/reads.gamp" --xg "${GF_DIR}/genefull.xg"
    --path-identity-ledger "${GF_FIX}/path_identity_ledger_bad_length.tsv"
    --out-dir "${GF_DIR}/identity_bad_length")
set_tests_properties(genefull_identity_xg_length_mismatch PROPERTIES
  LABELS "vg;pathtally;identity-ledger" FIXTURES_REQUIRED "genefull_annotated;genefull_reads"
  WILL_FAIL TRUE)

# The whole-body read gives each duplicate exon path score 30 and each duplicate transcript-body
# path score 40. MAX collapse calls U; summing the exon copies would incorrectly call S.
add_test(NAME genefull_ledger_transcript_body_max_not_sum
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/alias_max.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g_alias.tsv' \
    --body-t2g '${GF_DIR}/transcript_body.t2g.tsv' --bam-multigene all \
    --out-dir '${GF_DIR}/ledger_transcript_body_max' \
    --bam-out '${GF_DIR}/ledger_transcript_body_max/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/ledger_transcript_body_max/reads.bam' \
       | grep 'TX:Z:ENST_CANONICAL' | grep -q 'GL:Z:U'")
set_tests_properties(genefull_ledger_transcript_body_max_not_sum PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;transcript-body"
  FIXTURES_REQUIRED "genefull_annotated;genefull_alias_max_reads;genefull_transcript_body_t2g_ready")

# Independent raw-BODY MAX-vs-sum guard. TX_FULL's exon path scores 40. Each of TX_BODY's two
# raw body copies scores 30, so MAX keeps only TX_FULL:S; summing the copies would manufacture
# 60 and incorrectly replace it with TX_BODY:U.
add_test(NAME genefull_ledger_transcript_body_raw_body_max
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/alias_max.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${GF_FIX}/t2g_transcript_body_max.tsv' \
    --body-t2g '${GF_FIX}/body_t2g_transcript_body_max.tsv' --bam-multigene all \
    --out-dir '${GF_DIR}/transcript_body_raw_max' \
    --bam-out '${GF_DIR}/transcript_body_raw_max/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/transcript_body_raw_max/reads.bam' \
       > '${GF_DIR}/transcript_body_raw_max/read.sam' \
    && grep -q 'TX:Z:TX_FULL' '${GF_DIR}/transcript_body_raw_max/read.sam' \
    && grep -q 'GL:Z:S' '${GF_DIR}/transcript_body_raw_max/read.sam' \
    && ! grep -q 'TX_BODY' '${GF_DIR}/transcript_body_raw_max/read.sam'")
set_tests_properties(genefull_ledger_transcript_body_raw_body_max PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;transcript-body"
  FIXTURES_REQUIRED "genefull_annotated;genefull_alias_max_reads")

add_test(NAME genefull_transcript_body_gene_conflict
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${GF_DIR}/reads.gamp" --xg "${GF_DIR}/genefull.xg"
    --count-mode genefull --legacy-adapter hst-v1 --t2g "${GF_FIX}/t2g_alias.tsv"
    --body-t2g "${GF_FIX}/body_t2g_gene_conflict.tsv"
    --out-dir "${GF_DIR}/transcript_body_gene_conflict")
set_tests_properties(genefull_transcript_body_gene_conflict PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;transcript-body"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads" WILL_FAIL TRUE)

add_test(NAME genefull_transcript_body_missing_transcript
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${GF_DIR}/reads.gamp" --xg "${GF_DIR}/genefull.xg"
    --count-mode genefull --legacy-adapter hst-v1 --t2g "${GF_FIX}/t2g_alias.tsv"
    --body-t2g "${GF_FIX}/body_t2g_missing_transcript.tsv"
    --out-dir "${GF_DIR}/transcript_body_missing_transcript")
set_tests_properties(genefull_transcript_body_missing_transcript PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;transcript-body"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads" WILL_FAIL TRUE)

add_test(NAME genefull_transcript_body_mixed_rows
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${GF_DIR}/reads.gamp" --xg "${GF_DIR}/genefull.xg"
    --count-mode genefull --legacy-adapter hst-v1 --t2g "${GF_FIX}/t2g_alias.tsv"
    --body-t2g "${GF_FIX}/body_t2g_mixed.tsv"
    --out-dir "${GF_DIR}/transcript_body_mixed")
set_tests_properties(genefull_transcript_body_mixed_rows PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;transcript-body"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads" WILL_FAIL TRUE)

add_test(NAME genefull_transcript_body_path_collision
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${GF_DIR}/reads.gamp" --xg "${GF_DIR}/genefull.xg"
    --count-mode genefull --legacy-adapter hst-v1 --t2g "${GF_FIX}/t2g_alias.tsv"
    --body-t2g "${GF_FIX}/body_t2g_path_collision.tsv"
    --out-dir "${GF_DIR}/transcript_body_path_collision")
set_tests_properties(genefull_transcript_body_path_collision PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;transcript-body"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads" WILL_FAIL TRUE)

# Exact Ex50pAS needs per-alignment base overlap and Parent-specific exon/body provenance. Historical
# hst-v1 t2g/body-t2g inputs cannot supply that contract and must fail instead of emitting an
# approximate GL ledger.
add_test(NAME genefull_legacy_ex50pas_rejected
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${GF_DIR}/reads.gamp" --xg "${GF_DIR}/genefull.xg"
    --count-mode genefull_ex50pas --legacy-adapter hst-v1
    --t2g "${GF_FIX}/t2g.tsv" --body-t2g "${GF_DIR}/genefull.t2g.tsv"
    --out-dir "${GF_DIR}/legacy_ex50" --bam-out "${GF_DIR}/legacy_ex50/reads.bam"
    --bam-multigene all)
set_tests_properties(genefull_legacy_ex50pas_rejected PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;legacy-adapter"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads" WILL_FAIL TRUE)

# Exact STARsolo 2.7.11b GeneFull_Ex50pAS tier evidence. The fixture keeps two distinct locus
# Parents on one canonical transcript and covers E, P, exactly-50%-is-B, fully-exonic but
# splice-discordant-is-P, reverse orientation, and branched MultipathAlignments whose alternatives
# sit exactly five and six score points below top. The verifier checks the inclusive five-point
# boundary, the six-point exclusion, all six parallel BAM arrays, and every XP/XU ledger resolution.
set(EX50_DIR "${CMAKE_CURRENT_BINARY_DIR}/ex50_tier_work")
set(EX50_FIX "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/ex50_tier_smoke")

add_test(NAME genefull_ex50pas_setup
  COMMAND ${CMAKE_COMMAND} -E make_directory "${EX50_DIR}")
set_tests_properties(genefull_ex50pas_setup PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50"
  FIXTURES_SETUP genefull_ex50pas_dir)

add_test(NAME genefull_ex50pas_xg
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> convert -g '${EX50_FIX}/graph.gfa' -x > '${EX50_DIR}/graph.xg'")
set_tests_properties(genefull_ex50pas_xg PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50"
  FIXTURES_REQUIRED genefull_ex50pas_dir FIXTURES_SETUP genefull_ex50pas_inputs)

add_test(NAME genefull_ex50pas_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${EX50_FIX}/reads.gamp.json' > '${EX50_DIR}/reads.gamp'")
set_tests_properties(genefull_ex50pas_gamp PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50"
  FIXTURES_REQUIRED genefull_ex50pas_dir FIXTURES_SETUP genefull_ex50pas_inputs)

add_test(NAME genefull_ex50pas_rad_baseline
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --count-mode genefull --out-dir "${EX50_DIR}/baseline")
set_tests_properties(genefull_ex50pas_rad_baseline PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50"
  FIXTURES_REQUIRED genefull_ex50pas_inputs
  FIXTURES_SETUP genefull_ex50pas_baseline)

add_test(NAME genefull_ex50pas_exact
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --count-mode genefull_ex50pas --bam-multigene all
    --out-dir "${EX50_DIR}/exact" --bam-out "${EX50_DIR}/exact/reads.bam")
set_tests_properties(genefull_ex50pas_exact PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam"
  FIXTURES_REQUIRED genefull_ex50pas_inputs
  FIXTURES_SETUP genefull_ex50pas_exact_out)

# A generated production-ledger fixture has 64 lexical Parents and 64 canonical targets. This is
# enough work for adaptive Parent blocks and transcript geometry to use multiple workers at
# --threads 8. Unique paths make the lazy node and exact-edge caches miss.
# The driver clears its work root before rebuilding inputs, compares every persisted artifact from
# --threads 1 and 8 byte-for-byte in both production and legacy geometry, and proves parallel
# failures retain lexical Parent ordering without committing a partial output directory.
set(PT_PARALLEL_EXACT_INIT_DIR "${CMAKE_CURRENT_BINARY_DIR}/parallel_exact_init_work")
add_test(NAME genefull_ex50pas_parallel_init_determinism
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/parallel_exact_init_determinism.sh"
    "$<TARGET_FILE:panCollapse>" "${PT_VG}" "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/make_parallel_exact_init_fixture.py" "${CMAKE_COMMAND}"
    "${PT_PARALLEL_EXACT_INIT_DIR}")
set_tests_properties(genefull_ex50pas_parallel_init_determinism PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;threads;initialization")

# A reference-derived Parent allowlist is applied before both the top-score-minus-5 window and
# E/P/B rank selection. BODY_PARENT_2 is lower-ranked than EXON_PARENT_1 for `concord`; retaining
# only the body Parent must therefore reselect B/F rather than drop the already-projected E/F call.
add_test(NAME genefull_ex50pas_strict_allowlisted_compact
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --path-identity-ledger-sha256 c2b91c03743bc22f40a9cc1b55545e0332d7e02d47aa5bcd6a3f2dbe6c1efb32
    --strict-allowlisted-parents "${EX50_FIX}/strict_allowlisted_parents.txt"
    --strict-allowlisted-parents-sha256 8e4417ffa72d8dd300cc84eb212bd73e287fb39475916b4409edb53405153cee
    --count-mode genefull_ex50pas --bam-multigene all
    --compact-exact-count-bam forward --strand both
    --out-dir "${EX50_DIR}/strict_allowlisted_compact"
    --bam-out "${EX50_DIR}/strict_allowlisted_compact/reads.bam")
set_tests_properties(genefull_ex50pas_strict_allowlisted_compact PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam;policy"
  FIXTURES_REQUIRED genefull_ex50pas_inputs
  FIXTURES_SETUP genefull_ex50pas_strict_allowlisted_compact_out)

add_test(NAME genefull_ex50pas_strict_allowlisted_compact_verify
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/strict_allowlisted_compact_verify.py"
    "${EX50_DIR}/strict_allowlisted_compact/reads.bam"
    8e4417ffa72d8dd300cc84eb212bd73e287fb39475916b4409edb53405153cee)
set_tests_properties(genefull_ex50pas_strict_allowlisted_compact_verify PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam;policy"
  FIXTURES_REQUIRED genefull_ex50pas_strict_allowlisted_compact_out)

# v0.8 keeps D068's inclusive five-point filter by default but exposes one explicit opt-out.
# Disabled means no score pruning at all (the pre-D068 exact-evidence surface), not a zero-width
# window. The top-minus-six within-DAG and cross-record alternatives must therefore reappear.
add_test(NAME genefull_ex50pas_score_window_disabled
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --count-mode genefull_ex50pas --bam-multigene all --no-ex50-score-window
    --out-dir "${EX50_DIR}/score_window_disabled"
    --bam-out "${EX50_DIR}/score_window_disabled/reads.bam")
set_tests_properties(genefull_ex50pas_score_window_disabled PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam;policy"
  FIXTURES_REQUIRED genefull_ex50pas_inputs
  FIXTURES_SETUP genefull_ex50pas_score_window_disabled_out)

add_test(NAME genefull_ex50pas_score_window_optout_requires_exact_bam
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --no-ex50-score-window --out-dir "${EX50_DIR}/invalid_score_window_optout")
set_tests_properties(genefull_ex50pas_score_window_optout_requires_exact_bam PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;policy"
  FIXTURES_REQUIRED genefull_ex50pas_inputs WILL_FAIL TRUE)

add_test(NAME genefull_ex50pas_debug
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --count-mode genefull_ex50pas --bam-multigene all
    --out-dir "${EX50_DIR}/debug" --bam-out "${EX50_DIR}/debug/reads.bam"
    --debug-evidence-out "${EX50_DIR}/debug/evidence.tsv")
set_tests_properties(genefull_ex50pas_debug PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam;debug"
  FIXTURES_REQUIRED genefull_ex50pas_inputs
  FIXTURES_SETUP genefull_ex50pas_debug_out)

add_test(NAME genefull_ex50pas_requires_bam
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --count-mode genefull_ex50pas --bam-multigene all
    --out-dir "${EX50_DIR}/missing_bam")
set_tests_properties(genefull_ex50pas_requires_bam PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50"
  FIXTURES_REQUIRED genefull_ex50pas_inputs WILL_FAIL TRUE)

add_test(NAME genefull_ex50pas_requires_all
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --count-mode genefull_ex50pas
    --out-dir "${EX50_DIR}/missing_all" --bam-out "${EX50_DIR}/missing_all/reads.bam")
set_tests_properties(genefull_ex50pas_requires_all PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam"
  FIXTURES_REQUIRED genefull_ex50pas_inputs WILL_FAIL TRUE)

add_test(NAME genefull_ex50pas_bam_verify
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ex50_tier_bam_verify.py"
    "${EX50_DIR}/exact/reads.bam" "${EX50_FIX}/path_identity_ledger.tsv" window5)
set_tests_properties(genefull_ex50pas_bam_verify PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam"
  FIXTURES_REQUIRED genefull_ex50pas_exact_out)

add_test(NAME genefull_ex50pas_score_window_disabled_verify
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ex50_tier_bam_verify.py"
    "${EX50_DIR}/score_window_disabled/reads.bam"
    "${EX50_FIX}/path_identity_ledger.tsv" disabled)
set_tests_properties(genefull_ex50pas_score_window_disabled_verify PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam;policy"
  FIXTURES_REQUIRED genefull_ex50pas_score_window_disabled_out)

add_test(NAME genefull_ex50pas_debug_verify
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/debug_evidence_verify.py"
    "${EX50_DIR}/exact/reads.bam" "${EX50_DIR}/debug/reads.bam"
    "${EX50_DIR}/debug/evidence.tsv")
set_tests_properties(genefull_ex50pas_debug_verify PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;bam;debug"
  FIXTURES_REQUIRED "genefull_ex50pas_exact_out;genefull_ex50pas_debug_out")

add_test(NAME genefull_ex50pas_rad_unchanged
  COMMAND ${CMAKE_COMMAND} -E compare_files
    "${EX50_DIR}/baseline/map.rad" "${EX50_DIR}/exact/map.rad")
set_tests_properties(genefull_ex50pas_rad_unchanged PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50"
  FIXTURES_REQUIRED "genefull_ex50pas_baseline;genefull_ex50pas_exact_out")

add_test(NAME genefull_ex50pas_score_window_toggle_rad_unchanged
  COMMAND ${CMAKE_COMMAND} -E compare_files
    "${EX50_DIR}/exact/map.rad" "${EX50_DIR}/score_window_disabled/map.rad")
set_tests_properties(genefull_ex50pas_score_window_toggle_rad_unchanged PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;policy"
  FIXTURES_REQUIRED
    "genefull_ex50pas_exact_out;genefull_ex50pas_score_window_disabled_out")

add_test(NAME genefull_ex50pas_debug_rad_unchanged
  COMMAND ${CMAKE_COMMAND} -E compare_files
    "${EX50_DIR}/exact/map.rad" "${EX50_DIR}/debug/map.rad")
set_tests_properties(genefull_ex50pas_debug_rad_unchanged PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;debug"
  FIXTURES_REQUIRED "genefull_ex50pas_exact_out;genefull_ex50pas_debug_out")

# Threaded conversion must retain a deterministic persisted order. Use the exact Ex50 fixture
# because it emits RAD, BAM, summary, tx2gene, and the evidence sidecar in one small pass. One
# byte per chunk forces ordered RAD chunk merging without making the fixture appreciably larger.
set(EX50_THREADS_DIR "${CMAKE_CURRENT_BINARY_DIR}/ex50_thread_determinism_work")
add_test(NAME genefull_ex50pas_thread_determinism_setup
  COMMAND ${CMAKE_COMMAND} -E make_directory "${EX50_THREADS_DIR}")
set_tests_properties(genefull_ex50pas_thread_determinism_setup PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;threads"
  FIXTURES_REQUIRED genefull_ex50pas_inputs
  FIXTURES_SETUP genefull_ex50pas_thread_determinism_dir)

add_test(NAME genefull_ex50pas_thread_determinism
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/thread_determinism.sh"
    "$<TARGET_FILE:panCollapse>" "${EX50_DIR}/reads.gamp" "${EX50_DIR}/graph.xg"
    "${EX50_FIX}/path_identity_ledger.tsv" "${EX50_THREADS_DIR}")
set_tests_properties(genefull_ex50pas_thread_determinism PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;threads;multichunk;bam;debug"
  ENVIRONMENT "PANCOLLAPSE_MAX_CHUNK_BYTES=1"
  FIXTURES_REQUIRED "genefull_ex50pas_inputs;genefull_ex50pas_thread_determinism_dir"
  FIXTURES_SETUP genefull_ex50pas_thread_determinism_out)

# No --threads is the single-worker contract. The determinism runner snapshots both invocations
# after using identical output paths and forced chunking; every cmp must be fail-fast.
add_test(NAME genefull_ex50pas_default_threads_matches_one
  COMMAND /bin/sh -c "set -eu; for artifact in map.rad reads.bam summary.tsv tx2gene.tsv evidence.tsv; do \
    cmp '${EX50_THREADS_DIR}/threads_default/'\"\$artifact\" '${EX50_THREADS_DIR}/threads_1/'\"\$artifact\"; \
  done")
set_tests_properties(genefull_ex50pas_default_threads_matches_one PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;threads;bam"
  FIXTURES_REQUIRED genefull_ex50pas_thread_determinism_out)

# Validate the public thread-count contract independently of data processing.
add_test(NAME genefull_ex50pas_threads_zero_rejected
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --count-mode genefull_ex50pas --bam-multigene all --threads 0
    --out-dir "${EX50_THREADS_DIR}/zero" --bam-out "${EX50_THREADS_DIR}/zero/reads.bam")
set_tests_properties(genefull_ex50pas_threads_zero_rejected PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;threads"
  WILL_FAIL TRUE FIXTURES_REQUIRED genefull_ex50pas_inputs)

add_test(NAME genefull_ex50pas_threads_noninteger_rejected
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${EX50_DIR}/reads.gamp" --xg "${EX50_DIR}/graph.xg"
    --path-identity-ledger "${EX50_FIX}/path_identity_ledger.tsv"
    --count-mode genefull_ex50pas --bam-multigene all --threads nope
    --out-dir "${EX50_THREADS_DIR}/noninteger" --bam-out "${EX50_THREADS_DIR}/noninteger/reads.bam")
set_tests_properties(genefull_ex50pas_threads_noninteger_rejected PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;ex50;threads"
  WILL_FAIL TRUE FIXTURES_REQUIRED genefull_ex50pas_inputs)

# A cyclic transcript-body path may not abort the arm: its Parent keeps exon-layer evidence while
# a clean Parent on the same graph retains byte-equivalent exact Ex50 evidence.
set(CYCLIC_DIR "${CMAKE_CURRENT_BINARY_DIR}/cyclic_body_work")
set(CYCLIC_FIX "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cyclic_body_smoke")
add_test(NAME cyclic_body_setup COMMAND ${CMAKE_COMMAND} -E make_directory "${CYCLIC_DIR}")
set_tests_properties(cyclic_body_setup PROPERTIES LABELS "vg;pathtally;ledger;ex50;cyclic"
  FIXTURES_SETUP cyclic_body_dir)
add_test(NAME cyclic_body_inputs
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> convert -g '${CYCLIC_FIX}/graph.gfa' -x > '${CYCLIC_DIR}/graph.xg' && $<SHELL_PATH:${PT_VG}> view -J -K -k '${CYCLIC_FIX}/reads.gamp.json' > '${CYCLIC_DIR}/reads.gamp'")
set_tests_properties(cyclic_body_inputs PROPERTIES LABELS "vg;pathtally;ledger;ex50;cyclic"
  FIXTURES_REQUIRED cyclic_body_dir FIXTURES_SETUP cyclic_body_inputs_ready)
add_test(NAME cyclic_body_degrades_and_preserves_clean
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert --gamp '${CYCLIC_DIR}/reads.gamp' --xg '${CYCLIC_DIR}/graph.xg' --path-identity-ledger '${CYCLIC_FIX}/path_identity_ledger.tsv' --count-mode genefull_ex50pas --bam-multigene all --out-dir '${CYCLIC_DIR}/cyclic' --bam-out '${CYCLIC_DIR}/cyclic/reads.bam' --debug-evidence-out '${CYCLIC_DIR}/cyclic/debug.tsv' 2> '${CYCLIC_DIR}/cyclic.stderr' && '$<TARGET_FILE:panCollapse>' convert --gamp '${CYCLIC_DIR}/reads.gamp' --xg '${CYCLIC_DIR}/graph.xg' --path-identity-ledger '${CYCLIC_FIX}/path_identity_ledger_clean.tsv' --count-mode genefull_ex50pas --bam-multigene all --out-dir '${CYCLIC_DIR}/clean' --bam-out '${CYCLIC_DIR}/clean/reads.bam' && grep -qP '^body_paths_degraded\\t1$' '${CYCLIC_DIR}/cyclic/summary.tsv' && grep -qP '^body_path_degrade_reason\\trepeated_exonic_node$' '${CYCLIC_DIR}/cyclic/summary.tsv' && grep -q 'debug: degraded body path CYCLIC_BODY' '${CYCLIC_DIR}/cyclic.stderr' && '${Python3_EXECUTABLE}' '${CMAKE_CURRENT_SOURCE_DIR}/cyclic_body_verify.py' '${CYCLIC_DIR}/cyclic/reads.bam' '${CYCLIC_DIR}/clean/reads.bam'")
set_tests_properties(cyclic_body_degrades_and_preserves_clean PROPERTIES
  LABELS "vg;pathtally;ledger;ex50;cyclic;bam"
  FIXTURES_REQUIRED cyclic_body_inputs_ready)

add_test(NAME cyclic_body_gene_change_fails_closed
  COMMAND /bin/sh -c "if '$<TARGET_FILE:panCollapse>' convert --gamp '${CYCLIC_DIR}/reads.gamp' --xg '${CYCLIC_DIR}/graph.xg' --path-identity-ledger '${CYCLIC_FIX}/path_identity_ledger_gene_mismatch.tsv' --count-mode genefull_ex50pas --bam-multigene all --out-dir '${CYCLIC_DIR}/gene_change' --bam-out '${CYCLIC_DIR}/gene_change/reads.bam' 2> '${CYCLIC_DIR}/gene_change.stderr'; then exit 1; fi && grep -Fx 'panCollapse: refusing body-path degradation because linked Parent CYCLIC_EXON_PARENT changes counted gene from SHARED_GENE to OTHER_GENE' '${CYCLIC_DIR}/gene_change.stderr'")
set_tests_properties(cyclic_body_gene_change_fails_closed PROPERTIES
  LABELS "vg;pathtally;ledger;ex50;cyclic;bam"
  FIXTURES_REQUIRED cyclic_body_inputs_ready)

add_test(NAME cyclic_body_resolvable_gene_change_fails_closed
  COMMAND /bin/sh -c "if '$<TARGET_FILE:panCollapse>' convert --gamp '${CYCLIC_DIR}/reads.gamp' --xg '${CYCLIC_DIR}/graph.xg' --path-identity-ledger '${CYCLIC_FIX}/path_identity_ledger_clean_gene_mismatch.tsv' --count-mode genefull_ex50pas --bam-multigene all --out-dir '${CYCLIC_DIR}/resolvable_gene_change' --bam-out '${CYCLIC_DIR}/resolvable_gene_change/reads.bam' 2> '${CYCLIC_DIR}/resolvable_gene_change.stderr'; then exit 1; fi && grep -Fx 'panCollapse: path identity ledger body Parent CLEAN_BODY_PARENT changes counted gene from SHARED_GENE to OTHER_GENE' '${CYCLIC_DIR}/resolvable_gene_change.stderr'")
set_tests_properties(cyclic_body_resolvable_gene_change_fails_closed PROPERTIES
  LABELS "vg;pathtally;ledger;ex50;cyclic;bam"
  FIXTURES_REQUIRED cyclic_body_inputs_ready)

# A repeated body node is valid when exon orientation fixes one occurrence. The accepted model
# must carry that exact occurrence through its base geometry and into the emitted direction.
set(RESOLVED_BODY_DIR "${CMAKE_CURRENT_BINARY_DIR}/repeated_body_resolved_work")
set(RESOLVED_BODY_FIX "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/repeated_body_resolved")
add_test(NAME repeated_body_resolved_setup COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOLVED_BODY_DIR}")
set_tests_properties(repeated_body_resolved_setup PROPERTIES
  LABELS "vg;pathtally;ledger;ex50;cyclic" FIXTURES_SETUP repeated_body_resolved_dir)
add_test(NAME repeated_body_resolved_inputs
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> convert -g '${RESOLVED_BODY_FIX}/graph.gfa' -x > '${RESOLVED_BODY_DIR}/graph.xg' && $<SHELL_PATH:${PT_VG}> view -J -K -k '${RESOLVED_BODY_FIX}/reads.gamp.json' > '${RESOLVED_BODY_DIR}/reads.gamp'")
set_tests_properties(repeated_body_resolved_inputs PROPERTIES
  LABELS "vg;pathtally;ledger;ex50;cyclic"
  FIXTURES_REQUIRED repeated_body_resolved_dir FIXTURES_SETUP repeated_body_resolved_inputs_ready)
add_test(NAME repeated_body_resolved_occurrence_and_orientation
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert --gamp '${RESOLVED_BODY_DIR}/reads.gamp' --xg '${RESOLVED_BODY_DIR}/graph.xg' --path-identity-ledger '${RESOLVED_BODY_FIX}/path_identity_ledger.tsv' --count-mode genefull_ex50pas --bam-multigene all --out-dir '${RESOLVED_BODY_DIR}/out' --bam-out '${RESOLVED_BODY_DIR}/out/reads.bam' && grep -qP '^body_paths_degraded\\t0$' '${RESOLVED_BODY_DIR}/out/summary.tsv' && grep -qP '^body_path_degrade_reason\\tnone$' '${RESOLVED_BODY_DIR}/out/summary.tsv' && '${Python3_EXECUTABLE}' '${CMAKE_CURRENT_SOURCE_DIR}/repeated_body_resolved_verify.py' '${RESOLVED_BODY_DIR}/out/reads.bam'")
set_tests_properties(repeated_body_resolved_occurrence_and_orientation PROPERTIES
  LABELS "vg;pathtally;ledger;ex50;cyclic;bam"
  FIXTURES_REQUIRED repeated_body_resolved_inputs_ready)

# D060 cross-isoform fixture: GENE1 has TWO exon isoforms over the SAME 3-node body -- NM_1.1 (exons
# 1,3, skips node 2) and NM_1.2 (exons 1,2,3, a retained-intron isoform that keeps node 2 as exon).
# genefull_smoke's GENE1 has only one isoform, so it cannot exercise a transcript spanning the read
# without explaining it exon-only; this fixture can. exonread (nodes 1,3, never touching node 2) ties
# the exon-path SCORE for both isoforms equally, but D061 splice concordance separates them: the
# read's own alignment skips node 2 (edge 1<->3), which is NM_1.1's own intron (NM_1.1 owns that
# edge) but not one of NM_1.2's own splice edges (NM_1.2's path never skips anything, so it owns no
# edge at all) -- NM_1.1 stays spliced, NM_1.2 fails concordance and falls back to unspliced via the
# (D061-untouched) span/body test, which still brackets it. ambigread (node 2 only) makes no splice
# of its own (concordance is vacuous/unconstrained for it, so D061 does not change this one) and ties
# top via NM_1.2's own exon path (spliced), while NM_1.1's precomputed span still brackets it and
# NM_1.1's own exon path does not explain it (unspliced) -- the SAME gene ends up with a spliced AND
# an unspliced transcript, which count_cr derives as ambiguous.
set(ISO_DIR "${CMAKE_CURRENT_BINARY_DIR}/genefull_isoform_work")
set(ISO_FIX "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/genefull_isoform_smoke")

add_test(NAME genefull_isoform_setup COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_DIR}")
set_tests_properties(genefull_isoform_setup PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_SETUP genefull_isoform_dir)

add_test(NAME genefull_isoform_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${ISO_FIX}/reads.gamp.json' > '${ISO_DIR}/reads.gamp'")
set_tests_properties(genefull_isoform_gamp PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED genefull_isoform_dir FIXTURES_SETUP genefull_isoform_reads)

add_test(NAME genefull_isoform_annotate
  COMMAND /bin/sh -c "'${CMAKE_SOURCE_DIR}/scripts/make-gene-annotation.sh' \
    -x '${ISO_FIX}/graph.gfa' -n '${ISO_FIX}/annotation.gtf' -o '${ISO_DIR}/genefull' --vg '${PT_VG}'")
set_tests_properties(genefull_isoform_annotate PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED genefull_isoform_dir FIXTURES_SETUP genefull_isoform_annotated)

# D061 regression guard: a read that never touches the differentiating node (2) ties the exon-path
# SCORE for both isoforms equally, but only NM_1.1 owns the read's actual splice (edge 1<->3, its own
# intron). NM_1.1 stays spliced; NM_1.2 is DROPPED (absent) by the concordance gate -- a spliced read
# is not an unspliced read, so NM_1.2 is neither S nor U and GENE1 is NOT made ambiguous. (Pre-D061
# node-membership called both S; a naive gate that only tightened S would have demoted NM_1.2 to U,
# wrongly making the gene ambiguous and dropping this legitimately spliced read from Gene.)
add_test(NAME genefull_ledger_isoform_control
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${ISO_DIR}/reads.gamp' --xg '${ISO_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${ISO_FIX}/t2g.tsv' --body-t2g '${ISO_DIR}/genefull.t2g.tsv' \
    --out-dir '${ISO_DIR}/ledger' --bam-out '${ISO_DIR}/ledger/reads.bam' \
    && grep -qP '^emitted_groups\\t2$' '${ISO_DIR}/ledger/summary.tsv' \
    && grep -qP '^multigene_dropped_groups\\t0$' '${ISO_DIR}/ledger/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${ISO_DIR}/ledger/reads.bam' | grep -w exonread | grep -qP 'TX:Z:NM_1.1(\\t|\$)' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${ISO_DIR}/ledger/reads.bam' | grep -w exonread | grep -qP 'GL:Z:S(\\t|\$)'")
set_tests_properties(genefull_ledger_isoform_control PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED "genefull_isoform_annotated;genefull_isoform_reads")

# The cross-isoform case: NM_1.1 unspliced, NM_1.2 spliced, same gene -- one RAD record (single
# gene, not dropped as multi-gene despite 2 emitted transcripts), and count_cr (gene mode) would
# exclude GENE1 here as ambiguous while genefull keeps it. ambigread crosses no splice edge of its
# own (a single-node touch), so D061 concordance is unconstrained here and this case is unaffected.
add_test(NAME genefull_ledger_cross_isoform_ambiguous
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${ISO_DIR}/reads.gamp' --xg '${ISO_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${ISO_FIX}/t2g.tsv' --body-t2g '${ISO_DIR}/genefull.t2g.tsv' \
    --out-dir '${ISO_DIR}/ledger' --bam-out '${ISO_DIR}/ledger/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${ISO_DIR}/ledger/reads.bam' | grep ambigread | grep -q 'TX:Z:NM_1.1;NM_1.2' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${ISO_DIR}/ledger/reads.bam' | grep ambigread | grep -q 'GX:Z:GENE1;GENE1' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${ISO_DIR}/ledger/reads.bam' | grep ambigread | grep -q 'GL:Z:U;S' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${ISO_DIR}/ledger/reads.bam' | grep ambigread | grep -q 'XT:Z:GENE1'")
set_tests_properties(genefull_ledger_cross_isoform_ambiguous PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED "genefull_isoform_annotated;genefull_isoform_reads")

# D063 splice ownership must compare each canonical transcript's exon paths with ITS OWN body
# paths. All test exon paths traverse 1->3. NM_1.1's body is 1,2,3, so it owns that skip;
# NM_1.2's body is 1,3, so it does not. NM_4.1 has only reverse exon/body paths and guards
# per-transcript GD=R. NM_1.1 also has a tied reverse raw copy: its forward/reverse evidence ties,
# so deterministic forward fallback gives GD=F rather than graph-path iteration order deciding.
# NM_5.1's body is split across disconnected singleton 1 and 3 fragments: neither fragment contains
# both exon-edge endpoints, but canonical membership makes their union own 1->3 because no body path
# has adjacent 1,3. A pooled gene body
# would incorrectly assign the edge to NM_1.2. The pure ledger test separately guards nonmonotonic,
# reversed, and repeated-node geometry without changing fixture ids.
add_test(NAME genefull_ledger_transcript_body_splice_ownership
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${ISO_DIR}/reads.gamp' --xg '${ISO_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${ISO_FIX}/t2g_transcript_geometry.tsv' \
    --body-t2g '${ISO_FIX}/body_t2g_transcript_geometry.tsv' --bam-multigene all \
    --out-dir '${ISO_DIR}/transcript_geometry' --bam-out '${ISO_DIR}/transcript_geometry/reads.bam' \
    2> '${ISO_DIR}/transcript_geometry.stderr' \
    && grep -q 'evaluated_target_edges=4 owned_target_edges=3 fragment_only_target_edges=1 adjacent_vetoed_target_edges=1' \
       '${ISO_DIR}/transcript_geometry.stderr' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${ISO_DIR}/transcript_geometry/reads.bam' \
       | grep exonread > '${ISO_DIR}/transcript_geometry/exonread.sam' \
    && grep -q 'TX:Z:NM_1.1;NM_4.1;NM_5.1' '${ISO_DIR}/transcript_geometry/exonread.sam' \
    && grep -q 'GD:Z:F;R;F' '${ISO_DIR}/transcript_geometry/exonread.sam' \
    && grep -q 'GL:Z:S;S;S' '${ISO_DIR}/transcript_geometry/exonread.sam' \
    && ! grep -q 'NM_1.2' '${ISO_DIR}/transcript_geometry/exonread.sam'")
set_tests_properties(genefull_ledger_transcript_body_splice_ownership PROPERTIES
  LABELS "vg;pathtally;genefull;ledger;isoform;transcript-body"
  FIXTURES_REQUIRED "genefull_isoform_annotated;genefull_isoform_reads")

# D058: --bam-multigene all. A dedicated fixture with TWO overlapping genes -- GENE1 (exons 1,3;
# intron/body node 2) and GENE2 (body = node 2 exactly, wholly inside GENE1's intron) -- so a read
# on the shared node 2 (multigeneread) is genuinely compatible with both gene bodies under a ledger
# mode; exonread (nodes 1,3) stays GENE1-only, a control proving single-gene reads are untouched.
set(MG_DIR "${CMAKE_CURRENT_BINARY_DIR}/multigene_bam_work")
set(MG_FIX "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/multigene_bam_smoke")

add_test(NAME multigene_bam_setup COMMAND ${CMAKE_COMMAND} -E make_directory "${MG_DIR}")
set_tests_properties(multigene_bam_setup PROPERTIES LABELS "vg;pathtally;genefull;multigene"
  FIXTURES_SETUP multigene_bam_dir)

add_test(NAME multigene_bam_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${MG_FIX}/reads.gamp.json' > '${MG_DIR}/reads.gamp'")
set_tests_properties(multigene_bam_gamp PROPERTIES LABELS "vg;pathtally;genefull;multigene"
  FIXTURES_REQUIRED multigene_bam_dir FIXTURES_SETUP multigene_bam_reads)

add_test(NAME multigene_bam_annotate
  COMMAND /bin/sh -c "'${CMAKE_SOURCE_DIR}/scripts/make-gene-annotation.sh' \
    -x '${MG_FIX}/graph.gfa' -n '${MG_FIX}/annotation.gtf' -o '${MG_DIR}/genefull' --vg '${PT_VG}'")
set_tests_properties(multigene_bam_annotate PROPERTIES LABELS "vg;pathtally;genefull;multigene"
  FIXTURES_REQUIRED multigene_bam_dir FIXTURES_SETUP multigene_bam_annotated)

# --bam-multigene omit (default): multigeneread is still absent from both RAD and BAM FEATURE
# evidence, but is retained as a barcode-only record for the all-read correction prior.
add_test(NAME multigene_bam_omit_drops_both
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${MG_FIX}/t2g.tsv' --body-t2g '${MG_DIR}/genefull.t2g.tsv' \
    --out-dir '${MG_DIR}/omit' --bam-out '${MG_DIR}/omit/reads.bam' --bam-multigene omit \
    && grep -qP '^emitted_groups\\t1$' '${MG_DIR}/omit/summary.tsv' \
    && grep -qP '^multigene_dropped_groups\\t1$' '${MG_DIR}/omit/summary.tsv' \
    && grep -qP '^barcode_only_bam_records\\t1$' '${MG_DIR}/omit/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' quickcheck '${MG_DIR}/omit/reads.bam' \
    && [ \"$('${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/omit/reads.bam' | wc -l)\" = 2 ] \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/omit/reads.bam' \
       | grep multigeneread | grep -q 'XB:Z:barcode_only' \
    && ! ('${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/omit/reads.bam' \
       | grep multigeneread | grep -q 'GX:Z:')")
set_tests_properties(multigene_bam_omit_drops_both PROPERTIES LABELS "vg;pathtally;genefull;multigene"
  FIXTURES_REQUIRED "multigene_bam_annotated;multigene_bam_reads")

# --bam-multigene all: the multi-gene read now FLOWS THROUGH to the BAM (proving it is not
# hard-dropped) carrying its full candidate-gene set (GX, no XT) for a downstream UMI-level rescue,
# while the RAD stays completely unchanged (same multigene_dropped_groups=1, same emitted_groups=1,
# byte-identical map.rad to a fresh --bam-multigene omit run) -- the ledger Unique/RAD contract from
# D057 is untouched, only the optional BAM gains the record. exonread (single-gene) is unaffected
# either way. Self-contained (runs its own omit comparison run rather than depending on another
# test's output directory, so it has no ordering dependency beyond the shared annotated fixture).
add_test(NAME multigene_bam_all_rescues_bam_only
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${MG_FIX}/t2g.tsv' --body-t2g '${MG_DIR}/genefull.t2g.tsv' \
    --out-dir '${MG_DIR}/all_cmp_omit' --bam-multigene omit \
    && '$<TARGET_FILE:panCollapse>' convert \
       --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' \
       --count-mode genefull --legacy-adapter hst-v1 --t2g '${MG_FIX}/t2g.tsv' --body-t2g '${MG_DIR}/genefull.t2g.tsv' \
       --out-dir '${MG_DIR}/all' --bam-out '${MG_DIR}/all/reads.bam' --bam-multigene all \
    && grep -qP '^emitted_groups\\t1$' '${MG_DIR}/all/summary.tsv' \
    && grep -qP '^multigene_dropped_groups\\t1$' '${MG_DIR}/all/summary.tsv' \
    && cmp '${MG_DIR}/all_cmp_omit/map.rad' '${MG_DIR}/all/map.rad' \
    && '${PANCOLLAPSE_SAMTOOLS}' quickcheck '${MG_DIR}/all/reads.bam' \
    && [ \"$('${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/all/reads.bam' | wc -l)\" = 2 ] \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/all/reads.bam' | grep multigeneread | grep -q 'GX:Z:GENE1;GENE2' \
    && ! ('${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/all/reads.bam' | grep multigeneread | grep -q 'XT:Z') \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/all/reads.bam' | grep -w exonread | grep -q 'XT:Z:GENE1'")
set_tests_properties(multigene_bam_all_rescues_bam_only PROPERTIES LABELS "vg;pathtally;genefull;multigene;bam"
  FIXTURES_REQUIRED "multigene_bam_annotated;multigene_bam_reads")

# D063 production path: convert the two generated gene-body rows into exact transcript-body rows,
# then prove --bam-multigene all carries the complete per-TX ledger through while RAD still applies
# Unique. count_cr receives NM_1.1:U and NM_2.1:S with no prematurely resolved XT gene.
add_test(NAME multigene_bam_transcript_body_all
  COMMAND /bin/sh -c "awk -F'\\t' 'BEGIN { OFS=\"\\t\" } \
      { tx = (\$2 == \"GENE1\" ? \"NM_1.1\" : \"NM_2.1\"); print \$1, \$2, tx }' \
      '${MG_DIR}/genefull.t2g.tsv' > '${MG_DIR}/transcript_body.t2g.tsv' \
    && '$<TARGET_FILE:panCollapse>' convert \
      --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' \
      --count-mode genefull --legacy-adapter hst-v1 --t2g '${MG_FIX}/t2g.tsv' \
      --body-t2g '${MG_DIR}/transcript_body.t2g.tsv' --bam-multigene all \
      --out-dir '${MG_DIR}/transcript_body_all' \
      --bam-out '${MG_DIR}/transcript_body_all/reads.bam' \
    && grep -qP '^emitted_groups\\t1$' '${MG_DIR}/transcript_body_all/summary.tsv' \
    && grep -qP '^multigene_dropped_groups\\t1$' '${MG_DIR}/transcript_body_all/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/transcript_body_all/reads.bam' \
       | grep multigeneread > '${MG_DIR}/transcript_body_all/multigeneread.sam' \
    && grep -q 'TX:Z:NM_1.1;NM_2.1' '${MG_DIR}/transcript_body_all/multigeneread.sam' \
    && grep -q 'GX:Z:GENE1;GENE2' '${MG_DIR}/transcript_body_all/multigeneread.sam' \
    && grep -q 'GL:Z:U;S' '${MG_DIR}/transcript_body_all/multigeneread.sam' \
    && ! grep -q 'XT:Z:' '${MG_DIR}/transcript_body_all/multigeneread.sam'")
set_tests_properties(multigene_bam_transcript_body_all PROPERTIES
  LABELS "vg;pathtally;genefull;multigene;bam;transcript-body"
  FIXTURES_REQUIRED "multigene_bam_annotated;multigene_bam_reads")

# score mode has no ledger drop to rescue from: --bam-multigene all is a true no-op there (RAD and
# BAM records identical to omit; only the recorded @PG command line differs, which this test does
# not compare). Confirms D058 does not touch the D048 default path.
add_test(NAME multigene_bam_all_noop_in_score_mode
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' --legacy-adapter hst-v1 --t2g '${MG_FIX}/t2g.tsv' \
    --out-dir '${MG_DIR}/score_omit' --bam-out '${MG_DIR}/score_omit/reads.bam' --bam-multigene omit \
    && '$<TARGET_FILE:panCollapse>' convert \
       --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' --legacy-adapter hst-v1 --t2g '${MG_FIX}/t2g.tsv' \
       --out-dir '${MG_DIR}/score_all' --bam-out '${MG_DIR}/score_all/reads.bam' --bam-multigene all \
    && cmp '${MG_DIR}/score_omit/map.rad' '${MG_DIR}/score_all/map.rad' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/score_omit/reads.bam' > '${MG_DIR}/score_omit_records.txt' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/score_all/reads.bam' > '${MG_DIR}/score_all_records.txt' \
    && cmp '${MG_DIR}/score_omit_records.txt' '${MG_DIR}/score_all_records.txt'")
set_tests_properties(multigene_bam_all_noop_in_score_mode PROPERTIES LABELS "vg;pathtally;genefull;multigene;bam"
  FIXTURES_REQUIRED "multigene_bam_annotated;multigene_bam_reads")

# D061: transcript A's exon vs transcript B's intron, on DIFFERENT genes -- the exact shape of the
# real bug (a TUBB1 haplotype isoform tied a read's top score by node-membership alone, while the
# read's actual splice was ATP5F1E's intron; TUBB1 owned none of it and STAR correctly calls GX:Z:-).
# GENE3/NM_3.1 (transcript A) is a single exon spanning all three nodes (1,2,3) contiguously, and --
# crucially -- is absent from annotation.gtf, so it has NO gene-body layer at all (mirroring a gene
# the ledger can only ever explain through its own exon path). GENE4/NM_4.1 (transcript B) IS
# annotated (1-30, exons 1-10 and 21-30), so vg rna embeds its gene body over nodes 1,2,3 -- node 2 is
# GENE4's own intron, so NM_4.1's exon path (1,3) owns the edge 1<->3. crossgeneread touches only
# nodes 1 and 3 (skips node 2 in its own alignment, same shape as genefull_isoform_smoke's exonread):
# NM_3.1's exon-path score ties NM_4.1's (both cross the same two nodes), so pre-D061 node-membership
# would call BOTH spliced. D061 concordance denies NM_3.1: it does not own the 1<->3 edge (its own
# path never skips node 2), so it fails the gate -- and, having no body layer to fall back to
# unspliced, is dropped entirely rather than merely demoted (matching the real TUBB1 finding: the
# non-owning transcript gets no Gene credit at all). NM_4.1 owns the edge and stays spliced. Net: one
# emitted transcript (NM_4.1 = S), a single-gene call, not a two-gene multigene drop.
set(SC_DIR "${CMAKE_CURRENT_BINARY_DIR}/splice_concordance_work")
set(SC_FIX "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/splice_concordance_smoke")

add_test(NAME splice_concordance_setup COMMAND ${CMAKE_COMMAND} -E make_directory "${SC_DIR}")
set_tests_properties(splice_concordance_setup PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_SETUP splice_concordance_dir)

add_test(NAME splice_concordance_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${SC_FIX}/reads.gamp.json' > '${SC_DIR}/reads.gamp'")
set_tests_properties(splice_concordance_gamp PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED splice_concordance_dir FIXTURES_SETUP splice_concordance_reads)

add_test(NAME splice_concordance_annotate
  COMMAND /bin/sh -c "'${CMAKE_SOURCE_DIR}/scripts/make-gene-annotation.sh' \
    -x '${SC_FIX}/graph.gfa' -n '${SC_FIX}/annotation.gtf' -o '${SC_DIR}/genefull' --vg '${PT_VG}'")
set_tests_properties(splice_concordance_annotate PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED splice_concordance_dir FIXTURES_SETUP splice_concordance_annotated)

# Only NM_4.1 (the true owner of the read's splice edge) is emitted, as S; NM_3.1 (ties the exon
# score but does not own that edge, and has no body layer to fall back to unspliced) is entirely
# absent -- not merely demoted to U. Single gene, single transcript, no multigene drop: this is the
# "assert A is NOT S" case from the splice-concordance spec.
add_test(NAME genefull_ledger_splice_concordance_cross_gene
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${SC_DIR}/reads.gamp' --xg '${SC_DIR}/genefull.xg' \
    --count-mode genefull --legacy-adapter hst-v1 --t2g '${SC_FIX}/t2g.tsv' --body-t2g '${SC_DIR}/genefull.t2g.tsv' \
    --out-dir '${SC_DIR}/ledger' --bam-out '${SC_DIR}/ledger/reads.bam' \
    && grep -qP '^emitted_groups\\t1$' '${SC_DIR}/ledger/summary.tsv' \
    && grep -qP '^multigene_dropped_groups\\t0$' '${SC_DIR}/ledger/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${SC_DIR}/ledger/reads.bam' | grep crossgeneread | grep -q 'TX:Z:NM_4.1' \
    && ! ('${PANCOLLAPSE_SAMTOOLS}' view '${SC_DIR}/ledger/reads.bam' | grep crossgeneread | grep -q 'NM_3.1') \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${SC_DIR}/ledger/reads.bam' | grep crossgeneread | grep -q 'GL:Z:S' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${SC_DIR}/ledger/reads.bam' | grep crossgeneread | grep -q 'GX:Z:GENE4' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${SC_DIR}/ledger/reads.bam' | grep crossgeneread | grep -q 'XT:Z:GENE4'")
set_tests_properties(genefull_ledger_splice_concordance_cross_gene PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED "splice_concordance_annotated;splice_concordance_reads")
