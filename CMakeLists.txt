cmake_minimum_required(VERSION 3.25)

project(panCollapse VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

include(CTest)

if(PANCOLLAPSE_ENABLE_VG_TESTS)
  include("${PROJECT_SOURCE_DIR}/cmake/PanCollapseVg.cmake")
  add_subdirectory(src)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
