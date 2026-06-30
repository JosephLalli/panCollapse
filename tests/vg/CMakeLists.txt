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
  NAME phase3_reverse_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_reverse.gamp.json > reads_reverse.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_reverse_gamp_builder PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_mixed_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_mixed.gamp.json > reads_mixed.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_mixed_gamp_builder PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_recurrent_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_recurrent.gamp.json > reads_recurrent.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_recurrent_gamp_builder PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_two_groups_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_two_groups.gamp.json > reads_two_groups.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_two_groups_gamp_builder PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_multi_target_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_multi_target.gamp.json > reads_multi_target.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_multi_target_gamp_builder PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_no_compatible_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_no_compatible.gamp.json > reads_no_compatible.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_no_compatible_gamp_builder PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_bad_molecule_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_bad_molecule.gamp.json > reads_bad_molecule.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_bad_molecule_gamp_builder PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_bad_molecule_malformed_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_bad_molecule_malformed.gamp.json > reads_bad_molecule_malformed.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_bad_molecule_malformed_gamp_builder PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_bad_molecule_unsupported_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_bad_molecule_unsupported.gamp.json > reads_bad_molecule_unsupported.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_bad_molecule_unsupported_gamp_builder PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_bad_molecule_missing_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_bad_molecule_missing.gamp.json > reads_bad_molecule_missing.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_bad_molecule_missing_gamp_builder PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_reverse_path_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_revpath.gamp.json > reads_revpath.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_reverse_path_gamp_builder PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_sources
  FIXTURES_SETUP phase2_vertical_slice_data)

add_test(
  NAME phase3_n_base_gamp_builder
  COMMAND /bin/sh -c "$<SHELL_PATH:${PANCOLLAPSE_VG_ROOT}/bin/vg> view -J -K -k read_n_base.gamp.json > reads_n_base.gamp"
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_n_base_gamp_builder PROPERTIES
  LABELS "vg;phase3;molecule-identity"
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
  NAME phase3_reverse_panCollapse_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads_reverse.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_reverse"
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase3_reverse_panCollapse_convert PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase3_reverse_rad)

add_test(
  NAME phase3_mixed_orientation_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads_mixed.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_mixed"
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase3_mixed_orientation_convert PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase3_mixed_rad)

add_test(
  NAME phase3_mixed_orientation_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase3_mixed_orientation_verify.py"
    "${PANCOLLAPSE_PHASE2_DIR}/pc_mixed")
set_tests_properties(phase3_mixed_orientation_verify PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase3_mixed_rad)

add_test(
  NAME phase3_recurrent_group_failure
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/phase3_expect_group_recurrence_failure.sh"
    "$<TARGET_FILE:panCollapse>"
    "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_recurrent_group_failure PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_data)

add_test(
  NAME phase3_two_groups_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads_two_groups.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_two_groups"
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase3_two_groups_convert PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase3_two_groups_rad)

add_test(
  NAME phase3_two_groups_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase3_grouping_verify.py"
    two-groups
    "${PANCOLLAPSE_PHASE2_DIR}/pc_two_groups")
set_tests_properties(phase3_two_groups_verify PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase3_two_groups_rad)

add_test(
  NAME phase3_multi_target_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads_multi_target.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation_multi.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse_multi.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_multi_target"
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase3_multi_target_convert PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase3_multi_target_rad)

add_test(
  NAME phase3_multi_target_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase3_grouping_verify.py"
    multi-target
    "${PANCOLLAPSE_PHASE2_DIR}/pc_multi_target")
set_tests_properties(phase3_multi_target_verify PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase3_multi_target_rad)

add_test(
  NAME phase3_no_compatible_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads_no_compatible.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_no_compatible"
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase3_no_compatible_convert PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase3_no_compatible_rad)

add_test(
  NAME phase3_no_compatible_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase3_grouping_verify.py"
    no-compatible
    "${PANCOLLAPSE_PHASE2_DIR}/pc_no_compatible")
set_tests_properties(phase3_no_compatible_verify PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase3_no_compatible_rad)

add_test(
  NAME phase3_bad_molecule_skip_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads_bad_molecule.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_bad_molecule_skip"
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase3_bad_molecule_skip_convert PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase3_bad_molecule_skip_rad)

add_test(
  NAME phase3_bad_molecule_skip_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase3_grouping_verify.py"
    bad-molecule-skip
    "${PANCOLLAPSE_PHASE2_DIR}/pc_bad_molecule_skip")
set_tests_properties(phase3_bad_molecule_skip_verify PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase3_bad_molecule_skip_rad)

add_test(
  NAME phase3_bad_molecule_malformed_strict_failure
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/phase3_expect_molecule_identity_failure.sh"
    "$<TARGET_FILE:panCollapse>"
    "${PANCOLLAPSE_PHASE2_DIR}"
    reads_bad_molecule_malformed.gamp
    raw_molecule_malformed_groups)
set_tests_properties(phase3_bad_molecule_malformed_strict_failure PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_data)

add_test(
  NAME phase3_bad_molecule_unsupported_strict_failure
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/phase3_expect_molecule_identity_failure.sh"
    "$<TARGET_FILE:panCollapse>"
    "${PANCOLLAPSE_PHASE2_DIR}"
    reads_bad_molecule_unsupported.gamp
    raw_molecule_unsupported_groups)
set_tests_properties(phase3_bad_molecule_unsupported_strict_failure PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_data)

add_test(
  NAME phase3_bad_molecule_missing_strict_failure
  COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/phase3_expect_molecule_identity_failure.sh"
    "$<TARGET_FILE:panCollapse>"
    "${PANCOLLAPSE_PHASE2_DIR}"
    reads_bad_molecule_missing.gamp
    raw_molecule_missing_groups)
set_tests_properties(phase3_bad_molecule_missing_strict_failure PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_data)

add_test(
  NAME phase3_reverse_path_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads_revpath.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation_revpath.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse_revpath.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_revpath"
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase3_reverse_path_convert PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase3_reverse_path_rad)

add_test(
  NAME phase3_reverse_path_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase3_grouping_verify.py"
    reverse-path
    "${PANCOLLAPSE_PHASE2_DIR}/pc_revpath")
set_tests_properties(phase3_reverse_path_verify PROPERTIES
  LABELS "vg;phase3;orientation"
  FIXTURES_REQUIRED phase3_reverse_path_rad)

add_test(
  NAME phase3_n_base_convert
  COMMAND panCollapse convert
    --gamp "${PANCOLLAPSE_PHASE2_DIR}/reads_n_base.gamp"
    --xg "${PANCOLLAPSE_PHASE2_DIR}/graph.xg"
    --gtf "${PANCOLLAPSE_PHASE2_DIR}/annotation.gtf"
    --collapse-manifest "${PANCOLLAPSE_PHASE2_DIR}/collapse.tsv"
    --out-dir "${PANCOLLAPSE_PHASE2_DIR}/pc_n_base"
    --raw-cb-length 16
    --raw-umi-length 12)
set_tests_properties(phase3_n_base_convert PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase2_vertical_slice_data
  FIXTURES_SETUP phase3_n_base_rad)

add_test(
  NAME phase3_n_base_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase3_grouping_verify.py"
    n-base
    "${PANCOLLAPSE_PHASE2_DIR}/pc_n_base")
set_tests_properties(phase3_n_base_verify PROPERTIES
  LABELS "vg;phase3;molecule-identity"
  FIXTURES_REQUIRED phase3_n_base_rad)

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

add_test(
  NAME phase3_reverse_alevin_permit
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" generate-permit-list -i pc_reverse -d rc -o af_reverse_permit -f 1 -t 1
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_reverse_alevin_permit PROPERTIES
  LABELS "vg;phase3;orientation;alevin-fry"
  FIXTURES_REQUIRED phase3_reverse_rad
  FIXTURES_SETUP phase3_reverse_permit)

add_test(
  NAME phase3_reverse_alevin_collate
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" collate -i af_reverse_permit -r pc_reverse -t 1
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_reverse_alevin_collate PROPERTIES
  LABELS "vg;phase3;orientation;alevin-fry"
  FIXTURES_REQUIRED phase3_reverse_permit
  FIXTURES_SETUP phase3_reverse_collated)

add_test(
  NAME phase3_reverse_alevin_quant
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" quant -i af_reverse_permit -m pc_reverse/tx2gene.tsv -o af_reverse_quant -r cr-like -t 1 --dump-eqclasses
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_reverse_alevin_quant PROPERTIES
  LABELS "vg;phase3;orientation;alevin-fry"
  FIXTURES_REQUIRED phase3_reverse_collated
  FIXTURES_SETUP phase3_reverse_quant)

add_test(
  NAME phase3_reverse_quant_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase2_quant_verify.py"
    "${PANCOLLAPSE_PHASE2_DIR}/pc_reverse"
    "${PANCOLLAPSE_PHASE2_DIR}/af_reverse_quant/alevin"
    reverse)
set_tests_properties(phase3_reverse_quant_verify PROPERTIES
  LABELS "vg;phase3;orientation;alevin-fry"
  FIXTURES_REQUIRED phase3_reverse_quant)

add_test(
  NAME phase3_multi_target_alevin_permit
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" generate-permit-list -i pc_multi_target -d either -o af_multi_target_permit -f 1 -t 1
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_multi_target_alevin_permit PROPERTIES
  LABELS "vg;phase3;orientation;alevin-fry"
  FIXTURES_REQUIRED phase3_multi_target_rad
  FIXTURES_SETUP phase3_multi_target_permit)

add_test(
  NAME phase3_multi_target_alevin_collate
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" collate -i af_multi_target_permit -r pc_multi_target -t 1
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_multi_target_alevin_collate PROPERTIES
  LABELS "vg;phase3;orientation;alevin-fry"
  FIXTURES_REQUIRED phase3_multi_target_permit
  FIXTURES_SETUP phase3_multi_target_collated)

add_test(
  NAME phase3_multi_target_alevin_quant
  COMMAND "${PANCOLLAPSE_ALEVIN_FRY}" quant -i af_multi_target_permit -m pc_multi_target/tx2gene.tsv -o af_multi_target_quant -r cr-like -t 1 --dump-eqclasses
  WORKING_DIRECTORY "${PANCOLLAPSE_PHASE2_DIR}")
set_tests_properties(phase3_multi_target_alevin_quant PROPERTIES
  LABELS "vg;phase3;orientation;alevin-fry"
  FIXTURES_REQUIRED phase3_multi_target_collated
  FIXTURES_SETUP phase3_multi_target_quant)

add_test(
  NAME phase3_multi_target_quant_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/phase3_grouping_verify.py"
    multi-target-quant
    "${PANCOLLAPSE_PHASE2_DIR}/af_multi_target_quant/alevin")
set_tests_properties(phase3_multi_target_quant_verify PROPERTIES
  LABELS "vg;phase3;orientation;alevin-fry"
  FIXTURES_REQUIRED phase3_multi_target_quant)
