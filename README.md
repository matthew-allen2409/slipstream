# Slipstream

[![CI](https://github.com/matthew-allen2409/slipstream/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/matthew-allen2409/slipstream/actions/workflows/ci.yml)

A single-instrument, volume-aware execution engine written in C++23.

## What it is

Slipstream sits between a market-data feed and an order-entry stream for one instrument. It:

- consumes a binary market-data feed over TCP and maintains a top-of-book (L1) view and a rolling
  VWAP over a configurable time window;
- receives candidate trades on a separate order-entry connection and decides, for each one,
  whether to accept or reject it based on:
  - **price band** — the trade must clear the rolling VWAP by a required edge (in basis points);
  - **size ceiling** — no single trade may exceed a maximum quantity;
  - **participation cap** — the firm's traded volume must stay within a fraction of market volume;
- ignores market data for any symbol other than the one it is configured for.

The engine core is deterministic and has no dependency on networking, so it can be driven and
tested directly without sockets. The hot path is designed to be allocation-free after warm-up.

Two replay clients are included that stream a CSV of quotes and trades into the engine over the
wire protocol, preserving the original inter-message timing.

> **Status:** early development. The project scaffold, core types and wire codec are in place;
> the engine logic, server event loop and benchmarks are in progress.

## Code layout

```
slipstream/
├── src/
│   ├── core/      Fixed-point Price (×10,000), Symbol, 128-bit integer helpers, RingWindow
│   ├── codec/     Wire-format offsets, decoded message types, FrameParser
│   ├── engine/    Engine, Decision / RejectReason (does not depend on net)
│   ├── net/       RAII file descriptors, NetError, feed transport interface
│   ├── perf/      Monotonic clock and LatencyRecorder
│   └── app/       Command-line parsing and main()
├── clients/
│   ├── common/    Shared CSV replay logic
│   ├── md_client/ Market-data replay client
│   └── oe_client/ Order-entry replay client
├── tests/         GoogleTest suite, allocation guard, adversarial scenario files
├── bench/         Engine microbenchmarks, flood client, run_bench.sh
├── data/          Sample quote-and-trade data (QandT.csv)
└── cmake/         Warning, sanitizer and dependency configuration
```

Library dependencies flow in one direction:

```
core ← codec ← engine
         ↑
        net
```

Because `engine` does not link `net`, all decision logic is unit-testable in isolation.

## Building

### Requirements

- Linux
- CMake ≥ 3.28 and Ninja
- GCC ≥ 14 or Clang ≥ 18 (for `<expected>` and `<print>`)
- GoogleTest — used from the system if available, otherwise fetched automatically

### Build and test

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
```

### Presets

| Preset        | Purpose                                                                 |
|---------------|-------------------------------------------------------------------------|
| `debug`       | GCC, debug symbols                                                      |
| `clang-debug` | Clang with lld                                                          |
| `asan-ubsan`  | GCC with AddressSanitizer + UndefinedBehaviorSanitizer                  |
| `release`     | `-O3 -march=native -fno-omit-frame-pointer` (benchmarks, flame graphs)  |
| `bench`       | `release` plus the Google Benchmark targets                             |

First-party code builds with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
-Werror`.

## Running

Start the engine, then the two replay clients (in separate terminals):

```sh
# Engine
./build/release/src/slipstream \
    --symbol SYNTH1 \
    --max-quantity 500 \
    --participation-cap 0.15 \
    --vwap-window-ms 30000 \
    --band-bps 25.5 \
    --md-host 127.0.0.1 --md-port 14200 \
    --oe-host 127.0.0.1 --oe-port 14300 \
    --transport tcp

# Market-data replay
./build/release/clients/md_client --port 14200

# Order-entry replay
./build/release/clients/oe_client --port 14300
```

The clients replay `data/QandT.csv` by default; pass `--csv PATH` to use another file. Once both
clients are connected, the engine signals the start of the session and both begin replaying on a
shared clock. Run any binary with `--help` for the full list of options.

### Engine options

| Option                       | Description                                         |
|------------------------------|-----------------------------------------------------|
| `--symbol SYM`               | Instrument to trade; all other symbols are discarded |
| `--max-quantity N`           | Ceiling on any single trade                         |
| `--participation-cap F`      | Fraction of market volume in `[0, 1]`, e.g. `0.15`  |
| `--vwap-window-ms N`         | Length of the trailing VWAP window                  |
| `--band-bps F`               | Required edge versus VWAP, in basis points          |
| `--md-host H --md-port P`    | Market-data listen address                          |
| `--oe-host H --oe-port P`    | Order-entry listen address                          |
| `--transport tcp\|udp\|grpc` | Feed transport (default `tcp`)                      |

### Benchmarks

```sh
./bench/run_bench.sh
```

This builds the `bench` preset, records basic machine information, and runs the engine
microbenchmarks pinned to a single core.

## Performance metrics

_To be added._
