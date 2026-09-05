cmake_minimum_required(VERSION 3.25)

project(panCollapse VERSION 0.10.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SIMDJSON REQUIRED IMPORTED_TARGET simdjson>=4.0)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd=1.5.7)

set(PANCOLLAPSE_ARROW_ROOT "" CACHE PATH
    "Pinned Arrow/Parquet C++ installation prefix (include/, libarrow, libparquet)")
find_package(Arrow 17.0.0 EXACT CONFIG QUIET)
find_package(Parquet 17.0.0 EXACT CONFIG QUIET)
if(TARGET Arrow::arrow_shared AND TARGET Parquet::parquet_shared)
  set(PANCOLLAPSE_ARROW_TARGET Arrow::arrow_shared)
  set(PANCOLLAPSE_PARQUET_TARGET Parquet::parquet_shared)
elseif(TARGET Arrow::arrow_static AND TARGET Parquet::parquet_static)
  set(PANCOLLAPSE_ARROW_TARGET Arrow::arrow_static)
  set(PANCOLLAPSE_PARQUET_TARGET Parquet::parquet_static)
else()
  find_path(PANCOLLAPSE_ARROW_INCLUDE_DIR arrow/api.h
    HINTS "${PANCOLLAPSE_ARROW_ROOT}/include" NO_DEFAULT_PATH)
  find_file(PANCOLLAPSE_ARROW_LIBRARY NAMES libarrow.so.1700 libarrow.so
    HINTS "${PANCOLLAPSE_ARROW_ROOT}" "${PANCOLLAPSE_ARROW_ROOT}/lib"
    NO_DEFAULT_PATH)
  find_file(PANCOLLAPSE_PARQUET_LIBRARY NAMES libparquet.so.1700 libparquet.so
    HINTS "${PANCOLLAPSE_ARROW_ROOT}" "${PANCOLLAPSE_ARROW_ROOT}/lib"
    NO_DEFAULT_PATH)
  if(NOT PANCOLLAPSE_ARROW_INCLUDE_DIR OR NOT PANCOLLAPSE_ARROW_LIBRARY OR
     NOT PANCOLLAPSE_PARQUET_LIBRARY)
    message(FATAL_ERROR
      "Pinned Arrow/Parquet C++ was not found; set PANCOLLAPSE_ARROW_ROOT")
  endif()
  add_library(panCollapse_arrow SHARED IMPORTED)
  set_target_properties(panCollapse_arrow PROPERTIES
    IMPORTED_LOCATION "${PANCOLLAPSE_ARROW_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${PANCOLLAPSE_ARROW_INCLUDE_DIR}")
  add_library(panCollapse_parquet SHARED IMPORTED)
  set_target_properties(panCollapse_parquet PROPERTIES
    IMPORTED_LOCATION "${PANCOLLAPSE_PARQUET_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${PANCOLLAPSE_ARROW_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES panCollapse_arrow)
  set(PANCOLLAPSE_ARROW_TARGET panCollapse_arrow)
  set(PANCOLLAPSE_PARQUET_TARGET panCollapse_parquet)
endif()

# Default to an optimized build with debug info when the user did not pick a
# configuration.  Single-config generators otherwise compile at -O0, which is
# several times slower on the scoring and per-node lookup hot paths (measured
# about 5x on a 1M-read MHC GAMP).  -O2 and -O3 measured within noise here, so
# RelWithDebInfo is preferred: same speed with symbols retained for diagnosis.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Build type (default RelWithDebInfo: optimized binary with debug symbols)" FORCE)
endif()

option(PANCOLLAPSE_ENABLE_VG_TESTS "Enable tests that compile and link local VG artifacts" ON)

# Warning flags for project-owned targets only.  Do NOT propagate to third-party
# or imported targets.  Each project target links this PRIVATE so the flags
# apply only to that target's own translation units.
add_library(panCollapse_warnings INTERFACE)
target_compile_options(panCollapse_warnings INTERFACE -Wall -Wextra)
add_library(panCollapse::warnings ALIAS panCollapse_warnings)

add_library(panCollapse_direct_count STATIC
  src/direct_count_compatibility.cpp
  src/direct_count.cpp
  src/direct_count_assignment.cpp
  src/direct_count_diagnostics.cpp
  src/direct_count_facts.cpp
  src/direct_count_output.cpp
  src/direct_count_runtime.cpp
)
target_include_directories(panCollapse_direct_count PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(panCollapse_direct_count PUBLIC OpenSSL::Crypto PkgConfig::SIMDJSON
  PkgConfig::ZSTD ${PANCOLLAPSE_ARROW_TARGET} ${PANCOLLAPSE_PARQUET_TARGET} ZLIB::ZLIB)
target_link_libraries(panCollapse_direct_count PRIVATE panCollapse::warnings)
add_library(panCollapse::direct_count ALIAS panCollapse_direct_count)

include(CTest)

if(PANCOLLAPSE_ENABLE_VG_TESTS)
  include("${PROJECT_SOURCE_DIR}/cmake/PanCollapseVg.cmake")
  add_subdirectory(src)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
