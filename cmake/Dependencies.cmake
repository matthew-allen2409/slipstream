# Third-party code is limited to tests and benchmarks. The core libraries depend on nothing.
include(FetchContent)

if(SLIPSTREAM_BUILD_TESTS)
    find_package(GTest 1.15 CONFIG QUIET)
    if(GTest_FOUND)
        message(STATUS "slipstream: using system GoogleTest ${GTest_VERSION}")
    else()
        message(STATUS "slipstream: system GoogleTest not found, fetching v1.17.0")
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE
            SYSTEM)
        FetchContent_MakeAvailable(googletest)
    endif()
    include(GoogleTest)
endif()

if(SLIPSTREAM_BUILD_BENCH)
    find_package(benchmark 1.8 CONFIG QUIET)
    if(NOT benchmark_FOUND)
        message(STATUS "slipstream: fetching Google Benchmark v1.9.1")
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.1
            GIT_SHALLOW TRUE
            SYSTEM)
        FetchContent_MakeAvailable(benchmark)
    endif()
endif()
