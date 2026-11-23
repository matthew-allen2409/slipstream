#include <cstddef>
#include <print>
#include <span>

#include "app/config.hpp"

int main(int argc, char** argv) {
    const auto config =
        slipstream::app::parse_args(std::span<char* const>(argv, static_cast<std::size_t>(argc)));
    if (!config) {
        if (config.error().kind == slipstream::app::ConfigError::Kind::HelpRequested) {
            std::print("{}", slipstream::app::usage());
            return 0;
        }
        std::println(stderr, "slipstream: {}", config.error().message);
        std::print(stderr, "{}", slipstream::app::usage());
        return 2;
    }
    // TODO(M2): construct EventLoop + Engine and run.
    return 0;
}
