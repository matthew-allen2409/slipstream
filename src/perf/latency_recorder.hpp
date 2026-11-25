#pragma once

/// @file
/// @brief Pre-allocated latency sample store.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slipstream::perf {

/// @brief Records tick-to-order samples.
///
/// All storage is reserved at construction; record() never allocates and drops samples (counting
/// them) once full.
///
/// TODO(M4): percentile computation at exit (off the hot path).
class LatencyRecorder {
public:
    /// @brief Reserves room for @p capacity samples.
    explicit LatencyRecorder(std::size_t capacity) { samples_.reserve(capacity); }

    /// @brief Stores one sample, or counts it as dropped if full.
    void record(std::uint64_t ns) noexcept {
        if (samples_.size() < samples_.capacity()) {
            samples_.push_back(ns);
        } else {
            ++dropped_;
        }
    }

    /// @brief Number of samples stored.
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    /// @brief Number of samples discarded because the recorder was full.
    [[nodiscard]] std::size_t dropped() const noexcept { return dropped_; }

private:
    std::vector<std::uint64_t> samples_;
    std::size_t dropped_ = 0;
};

}  // namespace slipstream::perf
