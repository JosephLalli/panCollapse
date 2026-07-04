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

# Self-contained end-to-end smoke: a tiny hand-authored graph + multipath GAMP go
# through panCollapse, and the independent oracle checks the RAD exactly.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(PANCOLLAPSE_ALEVIN_FRY alevin-fry HINTS "/mnt/ssd/lalli/usr/local/bin"
  DOC "alevin-fry executable for the RAD interoperability test")
if(NOT PANCOLLAPSE_ALEVIN_FRY)
  message(FATAL_ERROR "alevin-fry executable not found; set -DPANCOLLAPSE_ALEVIN_FRY=/path/to/alevin-fry")
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
