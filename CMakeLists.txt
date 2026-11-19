cmake_minimum_required(VERSION 3.20)
project(jobshop_repair LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_library(jsr_core src/instance.cpp src/schedule.cpp src/repair.cpp)
target_include_directories(jsr_core PUBLIC include)
target_compile_options(jsr_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jsr src/main.cpp)
target_link_libraries(jsr PRIVATE jsr_core)
target_compile_options(jsr PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(jsr_tests tests/test_main.cpp)
target_link_libraries(jsr_tests PRIVATE jsr_core)
target_compile_definitions(jsr_tests PRIVATE JSR_INSTANCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/instances")
add_test(NAME jsr_tests COMMAND jsr_tests)
