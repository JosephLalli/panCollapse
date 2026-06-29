include_guard(GLOBAL)

set(PANCOLLAPSE_VG_ROOT "/mnt/ssd/lalli/usr/local/src/vg" CACHE PATH
  "Root of the local VG checkout/build used by panCollapse tests")
set(PANCOLLAPSE_ABSL_PKGCONFIG "/mnt/ssd/lalli/.linuxbrew/Cellar/abseil/20260107.0/lib/pkgconfig" CACHE PATH
  "pkg-config directory for the Abseil libraries used by protobuf in the local VG build")

if(NOT EXISTS "${PANCOLLAPSE_VG_ROOT}/include/xg.hpp")
  message(FATAL_ERROR "PANCOLLAPSE_VG_ROOT does not look like a VG checkout: ${PANCOLLAPSE_VG_ROOT}")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${PANCOLLAPSE_VG_ROOT}")

if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
  set(ENV{PKG_CONFIG_PATH} "${PANCOLLAPSE_VG_ROOT}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
else()
  set(ENV{PKG_CONFIG_PATH} "${PANCOLLAPSE_VG_ROOT}/lib/pkgconfig")
endif()
if(EXISTS "${PANCOLLAPSE_ABSL_PKGCONFIG}")
  set(ENV{PKG_CONFIG_PATH} "${PANCOLLAPSE_ABSL_PKGCONFIG}:$ENV{PKG_CONFIG_PATH}")
endif()

find_package(OpenMP REQUIRED COMPONENTS CXX)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(libhandlegraph REQUIRED CONFIG)
find_package(VGio REQUIRED CONFIG)

pkg_check_modules(SDSL REQUIRED IMPORTED_TARGET sdsl-lite)
pkg_check_modules(DIVSUFSORT REQUIRED IMPORTED_TARGET libdivsufsort)
pkg_check_modules(DIVSUFSORT64 REQUIRED IMPORTED_TARGET libdivsufsort64)
pkg_check_modules(ABSL_LOG_INTERNAL_CHECK_OP REQUIRED IMPORTED_TARGET absl_log_internal_check_op)

add_library(panCollapse_xg STATIC IMPORTED GLOBAL)
set_target_properties(panCollapse_xg PROPERTIES
  IMPORTED_LOCATION "${PANCOLLAPSE_VG_ROOT}/lib/libxg.a"
  INTERFACE_INCLUDE_DIRECTORIES "${PANCOLLAPSE_VG_ROOT}/include")
add_library(panCollapse::xg ALIAS panCollapse_xg)

add_library(panCollapse_vg_boundary INTERFACE)
target_include_directories(panCollapse_vg_boundary INTERFACE
  "${PANCOLLAPSE_VG_ROOT}/include")
target_link_libraries(panCollapse_vg_boundary INTERFACE
  VGio::VGio
  libhandlegraph::handlegraph_shared
  panCollapse::xg
  PkgConfig::SDSL
  PkgConfig::DIVSUFSORT
  PkgConfig::DIVSUFSORT64
  PkgConfig::ABSL_LOG_INTERNAL_CHECK_OP
  OpenMP::OpenMP_CXX
  Threads::Threads
  m
  atomic)
add_library(panCollapse::vg_boundary ALIAS panCollapse_vg_boundary)
