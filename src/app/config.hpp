#pragma once

/// @file
/// @brief Command-line configuration for the slipstream binary.

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "engine/engine.hpp"

namespace slipstream::app {

/// @brief Feed transport selected with `--transport`.
enum class Transport : std::uint8_t { Tcp, Udp, Grpc };

/// @brief A host/port pair to listen on.
struct Endpoint {
    std::string host = "127.0.0.1";  ///< Listen address.
    std::uint16_t port = 0;          ///< Listen port.
};

/// @brief Fully parsed and validated startup configuration.
struct Config {
    engine::EngineConfig engine;                      ///< Engine thresholds.
    Endpoint md{.host = "127.0.0.1", .port = 14200};  ///< Market data listen address.
    Endpoint oe{.host = "127.0.0.1", .port = 14300};  ///< Order entry listen address.
    Transport transport = Transport::Tcp;             ///< Feed transport.
};

/// @brief Why parse_args() did not produce a Config.
///
/// Startup only, so a heap-allocated message is acceptable here (unlike the hot-path error enums).
struct ConfigError {
    /// @brief Help was requested, or the arguments were invalid.
    enum class Kind : std::uint8_t { HelpRequested, Invalid };
    Kind kind = Kind::Invalid;  ///< What went wrong.
    std::string message;        ///< Human-readable detail (empty for HelpRequested).
};

/// @brief Parses the program arguments into a Config.
/// @param args argv as a span, including the program name at index 0.
/// @return The config, or a ConfigError if help was requested or an argument is invalid.
[[nodiscard]] std::expected<Config, ConfigError> parse_args(std::span<char* const> args);

/// @brief The `--help` text.
[[nodiscard]] std::string_view usage() noexcept;

}  // namespace slipstream::app
