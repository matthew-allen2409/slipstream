# slipstream::sanitizers: enabled with -DSLIPSTREAM_SANITIZE=address,undefined (or thread).
add_library(slipstream_sanitizers INTERFACE)
add_library(slipstream::sanitizers ALIAS slipstream_sanitizers)

if(SLIPSTREAM_SANITIZE)
    target_compile_options(slipstream_sanitizers INTERFACE
        -fsanitize=${SLIPSTREAM_SANITIZE}
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all)
    target_link_options(slipstream_sanitizers INTERFACE -fsanitize=${SLIPSTREAM_SANITIZE})
endif()
