add_executable(vg_link_smoke vg_link_smoke.cpp)
target_link_libraries(vg_link_smoke PRIVATE panCollapse::vg_boundary)
add_test(NAME vg_link_smoke COMMAND vg_link_smoke)
set_tests_properties(vg_link_smoke PROPERTIES LABELS "vg;link")

# PathTally unit tests (self-contained; no external data).
add_executable(pathtally_score_test pathtally_score_test.cpp)
target_include_directories(pathtally_score_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(pathtally_score_test PRIVATE panCollapse::vg_boundary)
add_test(NAME pathtally_score_test COMMAND pathtally_score_test)
set_tests_properties(pathtally_score_test PROPERTIES LABELS "vg;pathtally")

add_executable(pathtally_pipeline_test pathtally_pipeline_test.cpp)
target_include_directories(pathtally_pipeline_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(pathtally_pipeline_test PRIVATE panCollapse::vg_boundary)
add_test(NAME pathtally_pipeline_test COMMAND pathtally_pipeline_test)
set_tests_properties(pathtally_pipeline_test PROPERTIES LABELS "vg;pathtally")

# Self-contained end-to-end smoke: a tiny hand-authored graph + multipath GAMP go
# through panCollapse, and the independent oracle checks the RAD exactly.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
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
