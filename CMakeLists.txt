cmake_minimum_required(VERSION 3.25)

project(panCollapse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PANCOLLAPSE_ENABLE_VG_TESTS "Enable tests that compile and link local VG artifacts" ON)

include(CTest)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
