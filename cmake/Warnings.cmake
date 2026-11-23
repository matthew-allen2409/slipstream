# slipstream::warnings: the warning set required by the spec (-Wall -Wextra -Wpedantic) plus
# the conversion/shadowing checks that catch fixed-point and wire-format mistakes early.
add_library(slipstream_warnings INTERFACE)
add_library(slipstream::warnings ALIAS slipstream_warnings)

target_compile_options(slipstream_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wold-style-cast
    -Wnon-virtual-dtor
    -Wnull-dereference
    -Wimplicit-fallthrough
    $<$<BOOL:${SLIPSTREAM_WERROR}>:-Werror>)
