#pragma once

#include <compare>
#include <cstdint>

namespace slipstream::core {

inline constexpr std::int64_t kPriceScale = 10'000;

struct Price {
    std::int64_t raw = 0;

    static constexpr Price from_raw(std::int64_t raw_value) noexcept { return Price{raw_value}; }

    friend constexpr auto operator<=>(Price, Price) noexcept = default;
};

// The codec copies Price to and from the wire as raw bytes.
static_assert(sizeof(Price) == sizeof(std::int64_t));

using Qty = std::uint32_t;

[[nodiscard]] constexpr double to_display(Price p) noexcept {
    return static_cast<double>(p.raw) / static_cast<double>(kPriceScale);
}

}  // namespace slipstream::core
