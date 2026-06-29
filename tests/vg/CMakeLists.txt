add_executable(vg_link_smoke vg_link_smoke.cpp)
target_link_libraries(vg_link_smoke PRIVATE panCollapse::vg_boundary)

add_test(NAME vg_link_smoke COMMAND vg_link_smoke)
set_tests_properties(vg_link_smoke PROPERTIES LABELS "vg;link")

add_executable(gamp_projection_fixture_writer gamp_projection_fixture_writer.cpp)

add_executable(gamp_xg_gtf_projection_smoke gamp_xg_gtf_projection_smoke.cpp)
target_link_libraries(gamp_xg_gtf_projection_smoke PRIVATE panCollapse::vg_boundary)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(PANCOLLAPSE_ALEVIN_FRY alevin-fry
  HINTS "/mnt/ssd/lalli/usr/local/bin"
  DOC "alevin-fry executable used for Phase 2 RAD interoperability tests")
if(NOT PANCOLLAPSE_ALEVIN_FRY)
  message(FATAL_ERROR "alevin-fry executable not found; set PANCOLLAPSE_ALEVIN_FRY")
endif()

add_executable(phase2_fixture_writer phase2_fixture_writer.cpp)

set(PANCOLLAPSE_PROJECTION_SMOKE_DIR "${CMAKE_CURRENT_BINARY_DIR}/projection_smoke_work")
set(PANCOLLAPSE_PHASE2_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase2_vertical_slice_work")

foreach(_phase2_strand antisense both)
  add_test(
    NAME phase2_reject_${_phase2_strand}_strand
    COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/phase2_expect_strand_tbi.sh"
      "$<TARGET_FILE:panCollapse>"
      "${_phase2_strand}")
  set_tests_properties(phase2_reject_${_phase2_strand}_strand PROPERTIES
    LABELS "vg;phase2")
endforeach()

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

add_test(
  NAME phase2_fixture_writer
  COMMAND phase2_fixture_writer "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase2_fixture_writer PROPERTIES
  LABELS "vg;phase2"
  FIXTURES_SETUP phase2_vertical_slice_sources)

add_test(
  NAME phase2_xg_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> convert -g graph.gfa -x > graph.xg"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase2_xg_builder PROPERTIES
  LABELS "vg;phase2"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase2_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read.gamp.json > reads.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase2_gamp_builder PROPERTIES
  LABELS "vg;phase2"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase2_panCollapse_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_out"
    --strand sense
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase2_panCollapse_convert PROPERTIES
  LABELS "vg;phase2"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase2_vertical_slice_rad)

add_test(
  NAME phase2_panCollapse_convert_repeat
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_out_repeat"
    --strand sense
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase2_panCollapse_convert_repeat PROPERTIES
  LABELS "vg;phase2"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase2_vertical_slice_rad)

add_test(
  NAME phase2_single_thread_reproducible
  COMMAND /bin/sh -c
    "cmp -s pc_out/map.rad pc_out_repeat/map.rad && cmp -s pc_out/tx2gene.tsv pc_out_repeat/tx2gene.tsv && cmp -s pc_out/summary.tsv pc_out_repeat/summary.tsv"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase2_single_thread_reproducible PROPERTIES
  LABELS "vg;phase2"
  FIXTURES_REQUIRED phase2_vertical_slice_rad)

add_test(
  NAME phase2_alevin_permit
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" generate-permit-list -i pc_out -d fw -o af_permit -f 1 -t 1
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase2_alevin_permit PROPERTIES
  LABELS "vg;phase2;alevin-fry"
  FIXTURES_REQUIRED phase2_vertical_slice_rad
  FIXTURES_SETUP phase2_vertical_slice_permit)

add_test(
  NAME phase2_alevin_collate
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" collate -i af_permit -r pc_out -t 1
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase2_alevin_collate PROPERTIES
  LABELS "vg;phase2;alevin-fry"
  FIXTURES_REQUIRED phase2_vertical_slice_permit
  FIXTURES_SETUP phase2_vertical_slice_collated)

add_test(
  NAME phase2_alevin_quant
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" quant -i af_permit -m pc_out/tx2gene.tsv -o af_quant -r cr-like -t 1 --dump-eqclasses
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase2_alevin_quant PROPERTIES
  LABELS "vg;phase2;alevin-fry"
  FIXTURES_REQUIRED phase2_vertical_slice_collated
  FIXTURES_SETUP phase2_vertical_slice_quant)

add_test(
  NAME phase2_quant_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase2_quant_verify.py"
    "${PANCOLLAPSE_PHASE2_DIR}/pc_out"
    "${PANCOLLAPSE_PHASE2_DIR}/af_quant/alevin")
set_tests_properties(phase2_quant_verify PROPERTIES
  LABELS "vg;phase2;alevin-fry"
  FIXTURES_REQUIRED phase2_vertical_slice_quant)
