# Unit tests for the Qt-free, libobs-free logic in src/stats-logic.cpp.
#
# Builds either on its own (cmake -S tests -B build_tests) with nothing but a
# C++17 compiler, or as part of the plugin build with -DENABLE_TESTS=ON.

cmake_minimum_required(VERSION 3.28...3.30)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(vertical-stats-for-obs-tests LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
  set(CMAKE_CXX_EXTENSIONS FALSE)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug)
  endif()
  enable_testing()
endif()

add_executable(stats-logic-tests main.cpp test-stats-logic.cpp "${CMAKE_CURRENT_SOURCE_DIR}/../src/stats-logic.cpp")
target_include_directories(stats-logic-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_compile_features(stats-logic-tests PRIVATE cxx_std_17)
target_compile_options(
  stats-logic-tests
  PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /utf-8> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

add_test(NAME stats-logic COMMAND stats-logic-tests)
