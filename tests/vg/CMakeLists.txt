add_executable(vg_link_smoke vg_link_smoke.cpp)
target_link_libraries(vg_link_smoke PRIVATE panCollapse::vg_boundary)

add_test(NAME vg_link_smoke COMMAND vg_link_smoke)
set_tests_properties(vg_link_smoke PROPERTIES LABELS "vg;link")

add_executable(gamp_projection_fixture_writer gamp_projection_fixture_writer.cpp)

add_executable(gamp_xg_gtf_projection_smoke gamp_xg_gtf_projection_smoke.cpp)
target_link_libraries(gamp_xg_gtf_projection_smoke PRIVATE panCollapse::vg_boundary)

set(PANCOLLAPSE_PROJECTION_SMOKE_DIR "${CMAKE_CURRENT_BINARY_DIR}/projection_smoke_work")

add_test(
  NAME gamp_projection_fixture_writer
  COMMAND gamp_projection_fixture_writer "${PANCOLLAPSE_PROJECTION_SMOKE_DIR}")
set_tests_properties(gamp_projection_fixture_writer PROPERTIES
  LABELS "vg;projection"
  FIXTURES_SETUP projection_smoke_sources)

add_test(
  NAME gamp_projection_xg_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> convert -g graph.gfa -x > graph.xg"
  WORKING_DIRECTORY "${PANCOLLAPSE_PROJECTION_SMOKE_DIR}")
set_tests_properties(gamp_projection_xg_builder PROPERTIES
  LABELS "vg;projection"
  FIXTURES_REQUIRED projection_smoke_sources
  FIXTURES_SETUP projection_smoke_data)

add_test(
  NAME gamp_projection_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read.gamp.json > reads.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PROJECTION_SMOKE_DIR}")
set_tests_properties(gamp_projection_gamp_builder PROPERTIES
  LABELS "vg;projection"
  FIXTURES_REQUIRED projection_smoke_sources
  FIXTURES_SETUP projection_smoke_data)

add_test(
  NAME gamp_xg_gtf_projection_smoke
  COMMAND gamp_xg_gtf_projection_smoke
    "${PANCOLLAPSE_PROJECTION_SMOKE_DIR}/graph.xg"
    "${PANCOLLAPSE_PROJECTION_SMOKE_DIR}/annotation.gtf"
    "${PANCOLLAPSE_PROJECTION_SMOKE_DIR}/reads.gamp")
set_tests_properties(gamp_xg_gtf_projection_smoke PROPERTIES
  LABELS "vg;projection"
  FIXTURES_REQUIRED projection_smoke_data)
