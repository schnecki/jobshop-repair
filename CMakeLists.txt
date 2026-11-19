cmake_minimum_required(VERSION 3.20)
project(jobshop_repair LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(JSR_SANITIZE "Build with address and undefined behaviour sanitizers" OFF)
option(JSR_WERROR "Treat warnings as errors" OFF)

add_library(jsr_warnings INTERFACE)
target_compile_options(jsr_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                              $<$<BOOL:${JSR_WERROR}>:-Werror>)
if(JSR_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(jsr_core
  src/instance.cpp
  src/schedule.cpp
  src/sequence.cpp
  src/local_search.cpp
  src/repair.cpp)
target_include_directories(jsr_core PUBLIC include)
target_link_libraries(jsr_core PRIVATE jsr_warnings)

add_executable(jsr src/main.cpp)
target_link_libraries(jsr PRIVATE jsr_core jsr_warnings)

# ---------- tests ----------
include(CTest)
if(BUILD_TESTING)
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(jsr_tests
    tests/test_instance.cpp
    tests/test_schedule.cpp
    tests/test_sequence.cpp
    tests/test_local_search.cpp
    tests/test_repair.cpp)
  target_link_libraries(jsr_tests PRIVATE jsr_core jsr_warnings GTest::gtest_main)
  target_compile_definitions(jsr_tests PRIVATE
    JSR_INSTANCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/instances")
  include(GoogleTest)
  gtest_discover_tests(jsr_tests)
endif()
