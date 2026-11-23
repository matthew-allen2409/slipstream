# Headers live beside their sources; includes are rooted at src/ (e.g. "codec/wire.hpp").
set(SLIPSTREAM_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/src")

# --- core: fixed point, symbol, int128, ring window (header-only) -----------------------------
add_library(slipstream_core INTERFACE)
add_library(slipstream::core ALIAS slipstream_core)
target_include_directories(slipstream_core INTERFACE "${SLIPSTREAM_INCLUDE_DIR}")
target_compile_features(slipstream_core INTERFACE cxx_std_23)

# --- codec: GCMD/1 + GCOE/1 wire format (the spec's libslipstream_codec) ----------------------
add_library(slipstream_codec STATIC codec/frame_parser.cpp)
add_library(slipstream::codec ALIAS slipstream_codec)
target_link_libraries(slipstream_codec PUBLIC slipstream::core)
slipstream_configure_target(slipstream_codec)

# --- engine: book, VWAP, decision rules. Must NOT depend on net. ------------------------------
add_library(slipstream_engine STATIC engine/engine.cpp)
add_library(slipstream::engine ALIAS slipstream_engine)
target_link_libraries(slipstream_engine PUBLIC slipstream::core slipstream::codec)
slipstream_configure_target(slipstream_engine)

# --- net: sockets, epoll loop, transports -----------------------------------------------------
add_library(slipstream_net STATIC net/fd.cpp)
add_library(slipstream::net ALIAS slipstream_net)
target_link_libraries(slipstream_net PUBLIC slipstream::codec)
slipstream_configure_target(slipstream_net)

# --- perf: clock + latency recording (header-only) --------------------------------------------
add_library(slipstream_perf INTERFACE)
add_library(slipstream::perf ALIAS slipstream_perf)
target_include_directories(slipstream_perf INTERFACE "${SLIPSTREAM_INCLUDE_DIR}")
target_compile_features(slipstream_perf INTERFACE cxx_std_23)

# --- app: CLI parsing (a library so tests can exercise it) + the slipstream executable --------
add_library(slipstream_cli STATIC app/config.cpp)
add_library(slipstream::cli ALIAS slipstream_cli)
target_link_libraries(slipstream_cli PUBLIC slipstream::engine)
slipstream_configure_target(slipstream_cli)

add_executable(slipstream app/main.cpp)
target_link_libraries(slipstream PRIVATE
    slipstream::cli slipstream::engine slipstream::net slipstream::perf)
slipstream_configure_target(slipstream)
