#pragma once

/// @file
/// @brief Fixed-point price and quantity types.

#include <compare>
#include <cstdint>

namespace slipstream::core {

/// @brief Price scale: prices are signed 64-bit fixed point scaled by 10'000.
///
/// 101.2500 is stored as 1'012'500. No floating point anywhere in accumulation or decisions;
/// to_display() exists for humans only.
inline constexpr std::int64_t kPriceScale = 10'000;

/// @brief Decimal places in a Price. kPriceScale == 10^kPriceDecimals (checked in parse_fixed.hpp).
inline constexpr unsigned kPriceDecimals = 4;

/// @brief A fixed-point price in units of 1 / kPriceScale.
struct Price {
    std::int64_t raw = 0;  ///< Scaled integer value.

    /// @brief Wraps an already-scaled integer.
    static constexpr Price from_raw(std::int64_t raw_value) noexcept { return Price{raw_value}; }

    friend constexpr auto operator<=>(Price, Price) noexcept = default;
};

// The codec copies a Price to the wire directly as its int64 raw value.
static_assert(sizeof(Price) == sizeof(std::int64_t));

/// @brief Order/trade quantity (whole units).
using Qty = std::uint32_t;

/// @brief Converts a Price to a double for display.
/// @warning Display only. Never feed the result back into a calculation.
[[nodiscard]] constexpr double to_display(Price p) noexcept {
    return static_cast<double>(p.raw) / static_cast<double>(kPriceScale);
}

}  // namespace slipstream::core
