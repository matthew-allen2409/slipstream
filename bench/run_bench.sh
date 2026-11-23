#!/usr/bin/env bash
# Reproducible benchmark run. TODO(M4): pin cores, record kernel/CPU/governor, run flood_client.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake --preset bench
cmake --build --preset bench

echo "== machine =="
uname -r
lscpu | grep -E 'Model name|^CPU\(s\)|MHz' || true
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true

echo "== engine microbenchmarks =="
taskset -c 2 ./build/bench/bench/slipstream_bench --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true
