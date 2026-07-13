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
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/out")
set_tests_properties(pathtally_smoke_convert PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED pathtally_smoke_inputs FIXTURES_SETUP pathtally_smoke_out)

# Independent oracle: exact RAD match plus an expected emitted-record count (3).
add_test(NAME pathtally_smoke_oracle
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/pathtally_oracle.py"
    "${PT_FIX}/reads.gamp.json" "${PT_SMOKE_DIR}/paths.gfa" "${PT_FIX}/t2g.tsv"
    "${PT_SMOKE_DIR}/out/map.rad" 3)
set_tests_properties(pathtally_smoke_oracle PROPERTIES LABELS "vg;pathtally"
  FIXTURES_REQUIRED "pathtally_smoke_out;pathtally_smoke_inputs")

# Optional BAM output. Same convert pass emits reads.bam alongside map.rad; the RAD must be
# byte-identical to the no-BAM smoke run, the BAM must be valid, and every record's tags and
# GX gene set must match the independent oracle.
add_test(NAME pathtally_bam_convert
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/out_bam"
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
    && grep -q '^@PG.*ID:panCollapse' '${PT_SMOKE_DIR}/bam_header.txt'")
set_tests_properties(pathtally_bam_header PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED pathtally_bam_out)

# RAD regression: the map.rad written with --bam-out must equal the no-BAM smoke run's.
add_test(NAME pathtally_bam_rad_identical
  COMMAND ${CMAKE_COMMAND} -E compare_files "${PT_SMOKE_DIR}/out_bam/map.rad" "${PT_SMOKE_DIR}/out/map.rad")
set_tests_properties(pathtally_bam_rad_identical PROPERTIES LABELS "vg;pathtally;bam"
  FIXTURES_REQUIRED "pathtally_bam_out;pathtally_smoke_out")

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
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/bam_count"
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
# --strand keeps only the matching orientation and counts the drops in strand_filtered_groups.
add_test(NAME pathtally_strand_forward
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${PT_SMOKE_DIR}/reads.gamp' --xg '${PT_SMOKE_DIR}/graph.xg' --t2g '${PT_FIX}/t2g.tsv' \
    --strand forward --out-dir '${PT_SMOKE_DIR}/strand_fwd' --bam-out '${PT_SMOKE_DIR}/strand_fwd/reads.bam' \
    && grep -qP '^emitted_groups\\t2$' '${PT_SMOKE_DIR}/strand_fwd/summary.tsv' \
    && grep -qP '^strand_filtered_groups\\t1$' '${PT_SMOKE_DIR}/strand_fwd/summary.tsv' \
    && ! '${PANCOLLAPSE_SAMTOOLS}' view '${PT_SMOKE_DIR}/strand_fwd/reads.bam' | grep -q readrev")
set_tests_properties(pathtally_strand_forward PROPERTIES LABELS "vg;pathtally;strand"
  FIXTURES_REQUIRED pathtally_smoke_inputs)

add_test(NAME pathtally_strand_reverse
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${PT_SMOKE_DIR}/reads.gamp' --xg '${PT_SMOKE_DIR}/graph.xg' --t2g '${PT_FIX}/t2g.tsv' \
    --strand reverse --out-dir '${PT_SMOKE_DIR}/strand_rev' --bam-out '${PT_SMOKE_DIR}/strand_rev/reads.bam' \
    && grep -qP '^emitted_groups\\t1$' '${PT_SMOKE_DIR}/strand_rev/summary.tsv' \
    && grep -qP '^strand_filtered_groups\\t2$' '${PT_SMOKE_DIR}/strand_rev/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${PT_SMOKE_DIR}/strand_rev/reads.bam' | grep -q readrev")
set_tests_properties(pathtally_strand_reverse PROPERTIES LABELS "vg;pathtally;strand"
  FIXTURES_REQUIRED pathtally_smoke_inputs)

# Multi-chunk path: force one chunk per record and confirm the oracle reads all
# chunks and still matches exactly.
add_test(NAME pathtally_smoke_convert_multichunk
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/out_mc")
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
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/af_out")
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
    --xg '${PT_SMOKE_DIR}/graph.xg' --t2g '${PT_FIX}/t2g.tsv' \
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
    --t2g '${PT_FIX}/t2g.tsv' --out-dir '${PT_SMOKE_DIR}/badmol_out' \
    && grep -qP '^raw_molecule_malformed_groups\\t1$' '${PT_SMOKE_DIR}/badmol_out/summary.tsv' \
    && grep -qP '^raw_molecule_skipped_groups\\t1$' '${PT_SMOKE_DIR}/badmol_out/summary.tsv' \
    && grep -qP '^emitted_groups\\t0$' '${PT_SMOKE_DIR}/badmol_out/summary.tsv'")
set_tests_properties(pathtally_badmol_skip PROPERTIES LABELS "vg;pathtally;molecule"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_badmol_inputs")

add_test(NAME pathtally_badmol_fail
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads_badmol.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/badmol_fail_out"
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
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/recur_out")
set_tests_properties(pathtally_recur_fail PROPERTIES LABELS "vg;pathtally;grouping"
  WILL_FAIL TRUE
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_recur_inputs")

# A failed run must not leave a valid-looking map.rad (N10: atomic temp-rename).
add_test(NAME pathtally_recur_no_partial
  COMMAND /bin/sh -c "! '$<TARGET_FILE:panCollapse>' convert \
    --gamp '${PT_SMOKE_DIR}/reads_recur.gamp' --xg '${PT_SMOKE_DIR}/graph.xg' \
    --t2g '${PT_FIX}/t2g.tsv' --out-dir '${PT_SMOKE_DIR}/recur_no_partial_out' \
    && test ! -f '${PT_SMOKE_DIR}/recur_no_partial_out/map.rad'")
set_tests_properties(pathtally_recur_no_partial PROPERTIES LABELS "vg;pathtally;grouping"
  FIXTURES_REQUIRED "pathtally_smoke_inputs;pathtally_recur_inputs")

# A GAMP aligned against a different graph (node IDs absent from the xg) must fail loudly.
add_test(NAME pathtally_badnode_gamp
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${PT_FIX}/reads_badnode.gamp.json' > '${PT_SMOKE_DIR}/reads_badnode.gamp'")
set_tests_properties(pathtally_badnode_gamp PROPERTIES LABELS "vg;pathtally;node-space"
  FIXTURES_REQUIRED pathtally_smoke_dir FIXTURES_SETUP pathtally_badnode_inputs)

add_test(NAME pathtally_badnode_fail
  COMMAND $<TARGET_FILE:panCollapse> convert
    --gamp "${PT_SMOKE_DIR}/reads_badnode.gamp" --xg "${PT_SMOKE_DIR}/graph.xg"
    --t2g "${PT_FIX}/t2g.tsv" --out-dir "${PT_SMOKE_DIR}/badnode_out")
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

# Spliced count: annotated graph + HST t2g. The intronic read is dropped (1 emitted, 1 no-compat).
add_test(NAME genefull_spliced_drops_intron
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' --t2g '${GF_FIX}/t2g.tsv' \
    --out-dir '${GF_DIR}/spliced' \
    && grep -qP '^emitted_groups\\t1$' '${GF_DIR}/spliced/summary.tsv' \
    && grep -qP '^no_compatible_transcript_groups\\t1$' '${GF_DIR}/spliced/summary.tsv'")
set_tests_properties(genefull_spliced_drops_intron PROPERTIES LABELS "vg;pathtally;genefull"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

# GeneFull count: the SAME annotated graph + the generated gene t2g. The intronic read is now
# called to its gene (2 emitted, 0 no-compat), carried in the BAM as GENE1. No mode flag.
add_test(NAME genefull_full_calls_intron
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' --t2g '${GF_DIR}/genefull.t2g.tsv' \
    --out-dir '${GF_DIR}/full' --bam-out '${GF_DIR}/full/reads.bam' \
    && grep -qP '^emitted_groups\\t2$' '${GF_DIR}/full/summary.tsv' \
    && grep -qP '^no_compatible_transcript_groups\\t0$' '${GF_DIR}/full/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' quickcheck '${GF_DIR}/full/reads.bam' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/full/reads.bam' | grep intronread | grep -q 'GX:Z:GENE1'")
set_tests_properties(genefull_full_calls_intron PROPERTIES LABELS "vg;pathtally;genefull;bam"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

# Ledger count modes (STARsolo/CellRanger Gene/GeneFull) on the both-annotated graph, reading the
# exon (HST) t2g and the gene-body t2g together. --count-mode gene drops the intronic read;
# genefull keeps it. (Rule semantics are unit-tested in pathtally_ledger_test.)
add_test(NAME genefull_ledger_gene
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode gene --t2g '${GF_FIX}/t2g.tsv' --body-t2g '${GF_DIR}/genefull.t2g.tsv' \
    --out-dir '${GF_DIR}/ledger_gene' \
    && grep -qP '^emitted_groups\\t1$' '${GF_DIR}/ledger_gene/summary.tsv' \
    && grep -qP '^no_compatible_transcript_groups\\t1$' '${GF_DIR}/ledger_gene/summary.tsv'")
set_tests_properties(genefull_ledger_gene PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED "genefull_annotated;genefull_reads")

add_test(NAME genefull_ledger_genefull
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${GF_DIR}/reads.gamp' --xg '${GF_DIR}/genefull.xg' \
    --count-mode genefull --t2g '${GF_FIX}/t2g.tsv' --body-t2g '${GF_DIR}/genefull.t2g.tsv' \
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

# Ex50pAS excludes a 100%-exonic ANTISENSE read: genefull keeps both a sense and an antisense
# exon read, genefull_ex50pas drops the antisense one.
add_test(NAME genefull_ledger_ex50pas_antisense
  COMMAND /bin/sh -c "$<SHELL_PATH:${PT_VG}> view -J -K -k '${GF_FIX}/reads_antisense.gamp.json' > '${GF_DIR}/anti.gamp' \
    && '$<TARGET_FILE:panCollapse>' convert --gamp '${GF_DIR}/anti.gamp' --xg '${GF_DIR}/genefull.xg' \
       --count-mode genefull --t2g '${GF_FIX}/t2g.tsv' --body-t2g '${GF_DIR}/genefull.t2g.tsv' \
       --out-dir '${GF_DIR}/anti_full' \
    && grep -qP '^emitted_groups\\t2$' '${GF_DIR}/anti_full/summary.tsv' \
    && '$<TARGET_FILE:panCollapse>' convert --gamp '${GF_DIR}/anti.gamp' --xg '${GF_DIR}/genefull.xg' \
       --count-mode genefull_ex50pas --t2g '${GF_FIX}/t2g.tsv' --body-t2g '${GF_DIR}/genefull.t2g.tsv' \
       --out-dir '${GF_DIR}/anti_ex50' --bam-out '${GF_DIR}/anti_ex50/reads.bam' \
    && grep -qP '^emitted_groups\\t1$' '${GF_DIR}/anti_ex50/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/anti_ex50/reads.bam' | grep -q exonsense \
    && ! '${PANCOLLAPSE_SAMTOOLS}' view '${GF_DIR}/anti_ex50/reads.bam' | grep -q exonanti")
set_tests_properties(genefull_ledger_ex50pas_antisense PROPERTIES LABELS "vg;pathtally;genefull;ledger"
  FIXTURES_REQUIRED genefull_annotated)

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

# --bam-multigene omit (default): multigeneread is dropped from BOTH RAD and BAM, exactly as
# before D058 -- this is the regression guard that the new flag did not change the old default.
add_test(NAME multigene_bam_omit_drops_both
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' \
    --count-mode genefull --t2g '${MG_FIX}/t2g.tsv' --body-t2g '${MG_DIR}/genefull.t2g.tsv' \
    --out-dir '${MG_DIR}/omit' --bam-out '${MG_DIR}/omit/reads.bam' --bam-multigene omit \
    && grep -qP '^emitted_groups\\t1$' '${MG_DIR}/omit/summary.tsv' \
    && grep -qP '^multigene_dropped_groups\\t1$' '${MG_DIR}/omit/summary.tsv' \
    && '${PANCOLLAPSE_SAMTOOLS}' quickcheck '${MG_DIR}/omit/reads.bam' \
    && [ \"$('${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/omit/reads.bam' | wc -l)\" = 1 ] \
    && ! '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/omit/reads.bam' | grep -q multigeneread")
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
    --count-mode genefull --t2g '${MG_FIX}/t2g.tsv' --body-t2g '${MG_DIR}/genefull.t2g.tsv' \
    --out-dir '${MG_DIR}/all_cmp_omit' --bam-multigene omit \
    && '$<TARGET_FILE:panCollapse>' convert \
       --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' \
       --count-mode genefull --t2g '${MG_FIX}/t2g.tsv' --body-t2g '${MG_DIR}/genefull.t2g.tsv' \
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

# score mode has no ledger drop to rescue from: --bam-multigene all is a true no-op there (RAD and
# BAM records identical to omit; only the recorded @PG command line differs, which this test does
# not compare). Confirms D058 does not touch the D048 default path.
add_test(NAME multigene_bam_all_noop_in_score_mode
  COMMAND /bin/sh -c "'$<TARGET_FILE:panCollapse>' convert \
    --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' --t2g '${MG_FIX}/t2g.tsv' \
    --out-dir '${MG_DIR}/score_omit' --bam-out '${MG_DIR}/score_omit/reads.bam' --bam-multigene omit \
    && '$<TARGET_FILE:panCollapse>' convert \
       --gamp '${MG_DIR}/reads.gamp' --xg '${MG_DIR}/genefull.xg' --t2g '${MG_FIX}/t2g.tsv' \
       --out-dir '${MG_DIR}/score_all' --bam-out '${MG_DIR}/score_all/reads.bam' --bam-multigene all \
    && cmp '${MG_DIR}/score_omit/map.rad' '${MG_DIR}/score_all/map.rad' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/score_omit/reads.bam' > '${MG_DIR}/score_omit_records.txt' \
    && '${PANCOLLAPSE_SAMTOOLS}' view '${MG_DIR}/score_all/reads.bam' > '${MG_DIR}/score_all_records.txt' \
    && cmp '${MG_DIR}/score_omit_records.txt' '${MG_DIR}/score_all_records.txt'")
set_tests_properties(multigene_bam_all_noop_in_score_mode PROPERTIES LABELS "vg;pathtally;genefull;multigene;bam"
  FIXTURES_REQUIRED "multigene_bam_annotated;multigene_bam_reads")
