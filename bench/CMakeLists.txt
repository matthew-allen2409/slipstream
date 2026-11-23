add_executable(slipstream_bench engine_bench.cpp)
target_link_libraries(slipstream_bench PRIVATE
    slipstream::engine slipstream::perf benchmark::benchmark_main)
slipstream_configure_target(slipstream_bench)

add_executable(flood_client flood_client.cpp)
target_link_libraries(flood_client PRIVATE slipstream::codec slipstream::net)
slipstream_configure_target(flood_client)
