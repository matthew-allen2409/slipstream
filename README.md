# Slipstream

A single-instrument, volume-aware execution engine in C++23. It consumes a binary market-data feed
over TCP, keeps an L1 book and a rolling VWAP, and checks each of the firm's historical trades
against a band, a size ceiling and a participation cap.

> Status: scaffold. Milestones M1–M4 are not yet implemented.

## Build

Requirements: Linux, CMake ≥ 3.28, Ninja, GCC ≥ 14 or Clang ≥ 18 (for `<expected>` and `<print>`).
GoogleTest is taken from the system if present, otherwise fetched.

```sh
cmake --preset release && cmake --build --preset release && ctest --preset release
```

| Preset | Purpose |
|---|---|
| `debug` | GCC, `-g` |
| `clang-debug` | Clang + lld |
| `asan-ubsan` | GCC with AddressSanitizer + UndefinedBehaviorSanitizer |
| `release` | `-O3 -march=native -fno-omit-frame-pointer` (benchmarks, flame graphs) |
| `bench` | `release` + Google Benchmark targets |

Core libraries build with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`.

## Run

```sh
./build/release/src/slipstream --symbol SYNTH1 --max-quantity 500 --participation-cap 0.15 \
    --vwap-window-ms 30000 --band-bps 25.5 \
    --md-host 127.0.0.1 --md-port 14200 --oe-host 127.0.0.1 --oe-port 14300 --transport tcp
./build/release/clients/md_client --port 14200
./build/release/clients/oe_client --port 14300
```

## Layout

| Path | Contents |
|---|---|
| `src/core` | `Price` fixed point (×10,000), `Symbol` (12-byte field), `i128`, `RingWindow` |
| `src/codec` | GCMD/1 + GCOE/1 wire offsets, decoded message types, `FrameParser` |
| `src/engine` | `Engine`, `Decision` / `RejectReason`. **Does not link `net`.** |
| `src/net` | RAII `Fd`, `NetError`, `IFeedTransport` |
| `src/perf` | `now_ns()`, `LatencyRecorder` |
| `src/app` | CLI parsing + `main` |
| `clients/` | MD and OE CSV replay clients |
| `tests/` | GoogleTest suite, `AllocGuard`, adversarial `scenarios/` |
| `bench/` | Engine microbenchmarks, flood client, `run_bench.sh` |

Dependency graph: `core ← codec ← engine`, `codec ← net`. The engine can be unit-tested without a socket.

## Design

_TODO: rolling-window structure and overflow policy, accumulator bounds, event loop._

## Assumptions

1. **Body sizes.** The spec gives Trade = 49 bytes and NewOrder = 42 bytes, but its field tables
   add up to **41** and **50**. This implementation follows the field tables (`codec/wire.hpp`).
2. **VWAP definition.** The spec's worked example is the mean of per-quote VWAPs,
   `mean((bq·bp + aq·ap)/(bq + aq))`, not `Σ(px·qty)/Σqty`. _TODO: state the choice._
3. **Participation window.** The CLI table says "cumulative since start"; the worked example uses
   the rolling window. _TODO: state the choice and defend it._
4. **Warm-up vs. always-reply.** §2.2 says send nothing before warm-up; M3 says every trade gets an
   `A`/`R`. _TODO: state the choice._
5. **Cross-socket ordering.** MD and OE are independent streams; trades are evaluated against the
   book as of arrival.

## Known issues

_TODO._

## Latency

_TODO: tick-to-order p50/p99/p99.9 with sample count, machine, kernel, governor, and pinning.
The CSV replay has only 1,058 SYNTH1 trades, too few for a meaningful p99.9, so headline numbers
come from `bench/flood_client`._

Allocation-free hot path: verified by `tests/support/alloc_guard` (counting replacement of global
`operator new`), asserting zero allocations across a post-warm-up replay.

## Transport comparison

_TODO (M4): TCP vs. UDP multicast / gRPC._
