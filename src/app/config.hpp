#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "engine/engine.hpp"

namespace slipstream::app {

enum class Transport : std::uint8_t { Tcp, Udp, Grpc };

struct Endpoint {
    std::string host = "127.0.0.1";
    std::uint16_t port = 0;
};

struct Config {
    engine::EngineConfig engine;
    Endpoint md{.host = "127.0.0.1", .port = 14200};
    Endpoint oe{.host = "127.0.0.1", .port = 14300};
    Transport transport = Transport::Tcp;
};

// Startup only, so a heap-allocated message is acceptable here (unlike the hot-path error enums).
struct ConfigError {
    enum class Kind : std::uint8_t { HelpRequested, Invalid };
    Kind kind = Kind::Invalid;
    std::string message;
};

[[nodiscard]] std::expected<Config, ConfigError> parse_args(std::span<char* const> args);

[[nodiscard]] std::string_view usage() noexcept;

}  // namespace slipstream::app
