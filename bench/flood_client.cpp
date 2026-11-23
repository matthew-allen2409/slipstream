// Full-speed load generator for tick-to-order measurement: 10^6+ trades over loopback, so the
// p99.9 has enough samples behind it. The CSV replay (1,058 SYNTH1 trades) cannot support one.
#include <print>

int main() {
    // TODO(M4): connect MD + OE, pre-encode frames, blast them, report sample count.
    std::println(stderr, "flood_client: not implemented yet");
    return 1;
}
