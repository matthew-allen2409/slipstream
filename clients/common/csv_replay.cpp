#include "csv_replay.hpp"

#include <cstddef>
#include <span>

namespace slipstream::replay {

bool wants_help(int argc, char** argv) noexcept {
    const std::span<char*> args(argv, static_cast<std::size_t>(argc));
    for (const char* arg : args.subspan(args.empty() ? 0 : 1)) {
        const std::string_view a{arg};
        if (a == "-h" || a == "--help") {
            return true;
        }
    }
    return false;
}

std::string_view client_usage() noexcept {
    return R"(  --csv PATH    replay file (default data/QandT.csv)
  --host H      slipstream address (default 127.0.0.1)
  --port P      slipstream port
  -h, --help    show this message
)";
}

}  // namespace slipstream::replay
