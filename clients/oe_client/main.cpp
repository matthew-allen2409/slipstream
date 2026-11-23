// Order entry client: replays Trade rows to slipstream and prints NewOrder / Heartbeat /
// SessionControl messages it receives (both directions).
#include <print>

#include "csv_replay.hpp"

int main(int argc, char** argv) {
    if (slipstream::replay::wants_help(argc, argv)) {
        std::print("usage: oe_client [options]\n{}", slipstream::replay::client_usage());
        return 0;
    }
    // TODO(M1): connect, replay trades, read responses.
    std::println(stderr, "oe_client: not implemented yet");
    return 1;
}
