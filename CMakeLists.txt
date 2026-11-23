cmake_minimum_required(VERSION 3.28)

project(slipstream
    VERSION 0.1.0
    DESCRIPTION "Volume-aware execution engine (GC-DEV-04)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(SLIPSTREAM_BUILD_TESTS "Build the GoogleTest suite" ON)
option(SLIPSTREAM_BUILD_BENCH "Build benchmarks (fetches Google Benchmark)" OFF)
option(SLIPSTREAM_WERROR "Treat warnings as errors" ON)
set(SLIPSTREAM_SANITIZE "" CACHE STRING "Sanitizers to enable, e.g. address,undefined or thread")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Warnings)
include(Sanitizers)
include(Dependencies)

# Applies the project-wide warning and sanitizer settings to a first-party target.
function(slipstream_configure_target target)
    target_link_libraries(${target} PRIVATE slipstream::warnings slipstream::sanitizers)
endfunction()

add_subdirectory(src)
add_subdirectory(clients)

if(SLIPSTREAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SLIPSTREAM_BUILD_BENCH)
    add_subdirectory(bench)
endif()
