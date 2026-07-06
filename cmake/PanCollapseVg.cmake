include_guard(GLOBAL)

set(PANCOLLAPSE_VG_ROOT "" CACHE PATH
  "Root of the local VG checkout/build used by panCollapse (required). \
Pass -DPANCOLLAPSE_VG_ROOT=/path/to/vg at configure time.")
set(PANCOLLAPSE_ABSL_PKGCONFIG "" CACHE PATH
  "Optional: pkg-config directory for Abseil when it is not on the standard \
pkg-config search path (e.g. a non-default Homebrew or Linuxbrew Cellar). \
Leave empty to rely on PKG_CONFIG_PATH and CMAKE_PREFIX_PATH.")

if(NOT PANCOLLAPSE_VG_ROOT)
  message(FATAL_ERROR
    "PANCOLLAPSE_VG_ROOT is not set. "
    "Pass -DPANCOLLAPSE_VG_ROOT=/path/to/vg at configure time.")
endif()
if(NOT EXISTS "${PANCOLLAPSE_VG_ROOT}/include/xg.hpp")
  message(FATAL_ERROR "PANCOLLAPSE_VG_ROOT does not look like a VG checkout: ${PANCOLLAPSE_VG_ROOT}")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${PANCOLLAPSE_VG_ROOT}")

if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
  set(ENV{PKG_CONFIG_PATH} "${PANCOLLAPSE_VG_ROOT}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
else()
  set(ENV{PKG_CONFIG_PATH} "${PANCOLLAPSE_VG_ROOT}/lib/pkgconfig")
endif()
if(PANCOLLAPSE_ABSL_PKGCONFIG AND EXISTS "${PANCOLLAPSE_ABSL_PKGCONFIG}")
  set(ENV{PKG_CONFIG_PATH} "${PANCOLLAPSE_ABSL_PKGCONFIG}:$ENV{PKG_CONFIG_PATH}")
endif()

find_package(OpenMP REQUIRED COMPONENTS CXX)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(libhandlegraph REQUIRED CONFIG)
find_package(Protobuf REQUIRED)
if(NOT TARGET protobuf::libprotobuf)
  set(_PANCOLLAPSE_PROTOBUF_LOCATION "")
  if(DEFINED Protobuf_LIBRARY AND EXISTS "${Protobuf_LIBRARY}")
    set(_PANCOLLAPSE_PROTOBUF_LOCATION "${Protobuf_LIBRARY}")
  elseif(DEFINED Protobuf_LIBRARY_RELEASE AND EXISTS "${Protobuf_LIBRARY_RELEASE}")
    set(_PANCOLLAPSE_PROTOBUF_LOCATION "${Protobuf_LIBRARY_RELEASE}")
  endif()
  if(_PANCOLLAPSE_PROTOBUF_LOCATION STREQUAL "")
    message(FATAL_ERROR "Protobuf was found, but no libprotobuf library path was exported")
  endif()
  add_library(protobuf::libprotobuf UNKNOWN IMPORTED GLOBAL)
  set_target_properties(protobuf::libprotobuf PROPERTIES
    IMPORTED_LOCATION "${_PANCOLLAPSE_PROTOBUF_LOCATION}"
    INTERFACE_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIRS}")
endif()
find_package(VGio REQUIRED CONFIG)

pkg_check_modules(SDSL REQUIRED IMPORTED_TARGET sdsl-lite)
pkg_check_modules(DIVSUFSORT REQUIRED IMPORTED_TARGET libdivsufsort)
pkg_check_modules(DIVSUFSORT64 REQUIRED IMPORTED_TARGET libdivsufsort64)
pkg_check_modules(ABSL_LOG_INTERNAL_CHECK_OP REQUIRED IMPORTED_TARGET absl_log_internal_check_op)
# Backing library for absl::flat_hash_map, used by the per-group tally (pathtally.hpp).
pkg_check_modules(ABSL_RAW_HASH_SET REQUIRED IMPORTED_TARGET absl_raw_hash_set)
# htslib for the optional BAM output writer (src/main.cpp). Already present alongside vg.
pkg_check_modules(HTSLIB REQUIRED IMPORTED_TARGET htslib)

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
  PkgConfig::ABSL_RAW_HASH_SET
  PkgConfig::HTSLIB
  OpenMP::OpenMP_CXX
  Threads::Threads
  m
  atomic)
add_library(panCollapse::vg_boundary ALIAS panCollapse_vg_boundary)
