add_subdirectory(policy)

if(PANCOLLAPSE_ENABLE_VG_TESTS)
  include("${PROJECT_SOURCE_DIR}/cmake/PanCollapseVg.cmake")
  add_subdirectory(vg)
endif()
