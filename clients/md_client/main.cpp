// Market data client: replays Quote rows from the CSV to slipstream (one direction only).
#include <print>

#include "csv_replay.hpp"

int main(int argc, char** argv) {
    if (slipstream::replay::wants_help(argc, argv)) {
        std::print("usage: md_client [options]\n{}", slipstream::replay::client_usage());
        return 0;
    }
    // TODO(M1): connect, replay quotes on the wall-time-relative schedule.
    std::println(stderr, "md_client: not implemented yet");
    return 1;
}
