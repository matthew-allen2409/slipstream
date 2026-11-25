# Allocation counting replaces global operator new/delete. It is an OBJECT library so the
# replacement is always linked into the test binary (and never into slipstream itself).
add_library(slipstream_alloc_guard OBJECT support/alloc_guard.cpp)
target_include_directories(slipstream_alloc_guard PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
slipstream_configure_target(slipstream_alloc_guard)

add_executable(slipstream_tests
    codec/byte_io_test.cpp
    codec/codec_test.cpp
    codec/wire_test.cpp
    core/fixed_point_test.cpp
    core/int128_test.cpp
    core/symbol_test.cpp
    net/fd_test.cpp
    support/alloc_guard_test.cpp)

target_link_libraries(slipstream_tests PRIVATE
    slipstream_alloc_guard
    slipstream::codec
    slipstream::engine
    slipstream::net
    slipstream::perf
    GTest::gtest_main)
slipstream_configure_target(slipstream_tests)

target_compile_definitions(slipstream_tests PRIVATE
    SLIPSTREAM_SCENARIO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scenarios"
    SLIPSTREAM_DATA_DIR="${PROJECT_SOURCE_DIR}/data")

gtest_discover_tests(slipstream_tests DISCOVERY_MODE PRE_TEST)
