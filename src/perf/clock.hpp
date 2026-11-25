#pragma once

/// @file
/// @brief Low-overhead monotonic clock.

#include <cstdint>
#include <ctime>

namespace slipstream::perf {

/// @brief Monotonic nanoseconds via the vDSO (no syscall, about 20 ns).
///
/// Used for tick-to-order timing.
[[nodiscard]] inline std::uint64_t now_ns() noexcept {
    timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec);
}

}  // namespace slipstream::perf
