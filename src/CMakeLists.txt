add_library(panCollapse_graph_input STATIC graph_input.cpp)
target_include_directories(panCollapse_graph_input PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(panCollapse_graph_input PUBLIC panCollapse::vg_boundary)
target_link_libraries(panCollapse_graph_input PRIVATE panCollapse::warnings)
add_executable(panCollapse main.cpp)
target_compile_definitions(panCollapse PRIVATE PANCOLLAPSE_VERSION="${PROJECT_VERSION}")
target_link_libraries(panCollapse PRIVATE panCollapse_graph_input panCollapse::direct_count
  panCollapse::warnings Threads::Threads)
