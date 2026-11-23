#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace slipstream::core {

// What happens when a push arrives and the window is already at capacity.
enum class OverflowPolicy : std::uint8_t {
    Reject,       // push fails; caller decides (documented data loss)
    EvictOldest,  // drop the head early (window becomes shorter than vwap-window-ms)
    Abort,        // treat as a capacity-planning bug
};

// Fixed-capacity circular buffer backing the rolling VWAP and participation windows.
// Storage is inline, so constructing one allocates nothing.
//
// TODO(M2): push / evict_older_than / running sums, plus tests for each OverflowPolicy.
template <class T, std::size_t Capacity, OverflowPolicy Policy>
class RingWindow {
    static_assert(Capacity > 0 && std::has_single_bit(Capacity),
                  "Capacity must be a power of two so index wrap is a mask");

public:
    static constexpr std::size_t kCapacity = Capacity;
    static constexpr OverflowPolicy kPolicy = Policy;

    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    std::array<T, Capacity> slots_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
};

}  // namespace slipstream::core
