#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slipstream::perf {

// Records tick-to-order samples. All storage is reserved at construction; record() never
// allocates and drops samples (counting them) once full.
//
// TODO(M4): percentile computation at exit (off the hot path).
class LatencyRecorder {
public:
    explicit LatencyRecorder(std::size_t capacity) { samples_.reserve(capacity); }

    void record(std::uint64_t ns) noexcept {
        if (samples_.size() < samples_.capacity()) {
            samples_.push_back(ns);
        } else {
            ++dropped_;
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] std::size_t dropped() const noexcept { return dropped_; }

private:
    std::vector<std::uint64_t> samples_;
    std::size_t dropped_ = 0;
};

}  // namespace slipstream::perf
