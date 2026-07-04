add_executable(panCollapse main.cpp)
target_compile_definitions(panCollapse PRIVATE PANCOLLAPSE_VERSION="${PROJECT_VERSION}")
target_link_libraries(panCollapse PRIVATE panCollapse::vg_boundary panCollapse::warnings)
