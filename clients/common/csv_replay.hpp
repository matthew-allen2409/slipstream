#pragma once

/// @file
/// @brief Shared helpers for the CSV replay clients.

#include <cstdint>
#include <string>
#include <string_view>

namespace slipstream::replay {

/// @brief Shared CLI options for both replay clients.
struct ClientOptions {
    std::string csv_path = "data/QandT.csv";  ///< Replay file.
    std::string host = "127.0.0.1";           ///< slipstream address.
    std::uint16_t port = 0;                   ///< slipstream port.
};

/// @brief Returns true if --help / -h was passed.
[[nodiscard]] bool wants_help(int argc, char** argv) noexcept;

/// @brief The options section of the clients' `--help` text.
[[nodiscard]] std::string_view client_usage() noexcept;

// TODO(M1): CSV reader (skip '#' rows and the header), decimal -> fixed point without floats,
// HH:MM:SS.mmm -> ts_ns, and a wall-time-relative scheduler using clock_nanosleep(TIMER_ABSTIME).

}  // namespace slipstream::replay
