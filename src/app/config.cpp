#include "app/config.hpp"

namespace slipstream::app {

std::string_view usage() noexcept {
    return R"(usage: slipstream [options]
  --symbol SYM              instrument to trade (others are discarded)
  --max-quantity N          ceiling on any single trade
  --participation-cap F     fraction in [0,1], e.g. 0.15
  --vwap-window-ms N        trailing rolling VWAP window
  --band-bps F              required edge in basis points, e.g. 25.5
  --md-host H --md-port P   market data listen address
  --oe-host H --oe-port P   order entry listen address
  --transport tcp|udp|grpc  feed transport (default tcp)
  -h, --help                show this message
)";
}

std::expected<Config, ConfigError> parse_args(std::span<char* const> args) {
    for (const char* arg : args.subspan(args.empty() ? 0 : 1)) {
        const std::string_view a{arg};
        if (a == "-h" || a == "--help") {
            return std::unexpected(ConfigError{.kind = ConfigError::Kind::HelpRequested,
                                               .message = {}});
        }
    }
    // TODO(M2): parse and validate every flag, converting decimals to fixed point without floats.
    return std::unexpected(ConfigError{.kind = ConfigError::Kind::Invalid,
                                       .message = "argument parsing is not implemented yet"});
}

}  // namespace slipstream::app
