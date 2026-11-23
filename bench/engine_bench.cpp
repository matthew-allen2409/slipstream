// Engine-only microbenchmarks (no sockets): isolates decision cost from kernel cost.
#include <benchmark/benchmark.h>

#include "perf/clock.hpp"

namespace {

// Baseline: the cost of one timestamp, which bounds the resolution of tick-to-order numbers.
void BM_NowNs(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(slipstream::perf::now_ns());
    }
}
BENCHMARK(BM_NowNs);

// TODO(M4): BM_DecodeQuote, BM_VwapPushEvict, BM_DecideTrade, BM_EncodeNewOrder.

}  // namespace
