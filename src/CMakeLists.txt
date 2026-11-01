add_library(panCollapse_graph_input STATIC graph_input.cpp)
target_include_directories(panCollapse_graph_input PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(panCollapse_graph_input PUBLIC panCollapse::vg_boundary)
target_link_libraries(panCollapse_graph_input PRIVATE panCollapse::warnings)
target_link_libraries(panCollapse_direct_count PRIVATE OpenMP::OpenMP_CXX)
target_compile_definitions(panCollapse_direct_count PRIVATE PANCOLLAPSE_PARALLEL_INDEX_SORT=1)
add_library(panCollapse_prepared_count STATIC count_reference.cpp reference_assignment_projection.cpp
  node_score_resolver.cpp native_count_index.cpp direct_count_index.cpp direct_count_compile.cpp prepared_count.cpp)
target_include_directories(panCollapse_prepared_count PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(panCollapse_prepared_count PUBLIC panCollapse_graph_input panCollapse::direct_count
  Threads::Threads OpenMP::OpenMP_CXX)
target_link_libraries(panCollapse_prepared_count PRIVATE panCollapse::warnings)
add_executable(panCollapse main.cpp)
target_compile_definitions(panCollapse PRIVATE PANCOLLAPSE_VERSION="${PROJECT_VERSION}")
target_link_libraries(panCollapse PRIVATE panCollapse_prepared_count panCollapse_graph_input panCollapse::direct_count
  panCollapse::warnings Threads::Threads)

# Explicit reference-repair utility; never used as an implicit count fallback.
add_executable(pancollapse_repair_ledger_names ../scripts/repair_ledger_graph_names.cpp)
target_link_libraries(pancollapse_repair_ledger_names PRIVATE panCollapse::vg_boundary OpenSSL::Crypto)
add_test(NAME repair_ledger_graph_names COMMAND pancollapse_repair_ledger_names --self-test)
