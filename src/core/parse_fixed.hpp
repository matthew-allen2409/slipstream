#pragma once

/// @file
/// @brief Decimal string to fixed-point integer, using integer math only.

#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>
#include <type_traits>

#include "core/fixed_point.hpp"

namespace slipstream::core {

/// @brief Why a decimal string could not be parsed as a fixed-point value.
enum class FixedParseError : std::uint8_t {
    Empty,            ///< Input is the empty string.
    InvalidFormat,    ///< Anything that is not [-]digits[.digits], or '-' for an unsigned type.
    TooManyDecimals,  ///< More fractional digits than the target precision (would lose precision).
    OutOfRange,       ///< Value does not fit in the target integer type.
};

/// @brief Human-readable description of a FixedParseError.
[[nodiscard]] constexpr std::string_view to_string(FixedParseError e) noexcept {
    switch (e) {
        case FixedParseError::Empty: return "empty input";
        case FixedParseError::InvalidFormat: return "not a decimal number";
        case FixedParseError::TooManyDecimals: return "too many decimal places";
        case FixedParseError::OutOfRange: return "value out of range";
    }
    return "unknown parse error";
}

namespace detail {

/// @brief 10^n. Compile-time only.
[[nodiscard]] consteval std::uint64_t pow10(unsigned n) noexcept {
    std::uint64_t result = 1;
    for (unsigned i = 0; i < n; ++i) {
        result *= 10;
    }
    return result;
}

}  // namespace detail

/// @brief Integer types parse_fixed can produce. Excludes bool and character types.
template <typename T>
concept FixedPointRep = std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> &&
                        !std::same_as<T, char8_t> && !std::same_as<T, char16_t> &&
                        !std::same_as<T, char32_t> && !std::same_as<T, wchar_t> && sizeof(T) <= 8;

/// @brief Parses a decimal string into an integer scaled by 10^Decimals.
///
/// parse_fixed<std::int64_t, 4>("101.25") == 1'012'500
/// parse_fixed<std::uint32_t, 6>("0.15")  == 150'000
///
/// Accepted grammar: an optional '-' (signed T only), one or more digits, then optionally a '.'
/// followed by 1 to Decimals digits. No '+', whitespace, exponent, or bare "1." / ".5".
/// Domain limits (e.g. "must be in [0, 1]") are the caller's job; this only checks that the
/// value fits in T.
template <FixedPointRep T, unsigned Decimals>
    requires(Decimals <= 18)  // 10^Decimals must fit in uint64_t.
[[nodiscard]] constexpr std::expected<T, FixedParseError> parse_fixed(std::string_view str) noexcept {
    constexpr std::uint64_t scale = detail::pow10(Decimals);

    if (str.empty()) {
        return std::unexpected{FixedParseError::Empty};
    }

    const bool is_negative = str.starts_with('-');
    if (is_negative) {
        if constexpr (std::is_unsigned_v<T>) {
            return std::unexpected{FixedParseError::InvalidFormat};
        }
        str.remove_prefix(1);
    }

    // Largest magnitude allowed. Negative values get one extra (|INT_MIN| == INT_MAX + 1).
    const std::uint64_t limit =
        static_cast<std::uint64_t>(std::numeric_limits<T>::max()) + (is_negative ? 1u : 0u);
    const std::uint64_t max_whole = limit / scale;

    std::uint64_t whole = 0;
    std::uint64_t fraction = 0;
    bool has_whole_digit = false;
    unsigned fraction_digits = 0;
    bool decimal_seen = false;

    for (const char ch : str) {
        if (ch == '.') {
            if (decimal_seen) [[unlikely]] {
                return std::unexpected{FixedParseError::InvalidFormat};
            }
            decimal_seen = true;
            continue;
        }

        if (ch < '0' || ch > '9') [[unlikely]] {
            return std::unexpected{FixedParseError::InvalidFormat};
        }

        const auto digit = static_cast<std::uint64_t>(ch - '0');

        if (!decimal_seen) {
            if (whole > (max_whole - digit) / 10) [[unlikely]] {
                return std::unexpected{FixedParseError::OutOfRange};
            }
            whole = whole * 10 + digit;
            has_whole_digit = true;
        } else {
            if (fraction_digits >= Decimals) [[unlikely]] {
                return std::unexpected{FixedParseError::TooManyDecimals};
            }
            fraction = fraction * 10 + digit;
            ++fraction_digits;
        }
    }

    if (!has_whole_digit || (decimal_seen && fraction_digits == 0)) [[unlikely]] {
        return std::unexpected{FixedParseError::InvalidFormat};
    }

    for (; fraction_digits < Decimals; ++fraction_digits) {
        fraction *= 10;
    }

    const std::uint64_t scaled_whole = whole * scale;
    if (fraction > limit - scaled_whole) [[unlikely]] {
        return std::unexpected{FixedParseError::OutOfRange};
    }
    const std::uint64_t magnitude = scaled_whole + fraction;

    return static_cast<T>(is_negative ? 0 - magnitude : magnitude);
}

/// @brief Parses a decimal string such as "101.25" or "-3.1" into a Price.
[[nodiscard]] constexpr std::expected<Price, FixedParseError> parse_price(std::string_view str) noexcept {
    return parse_fixed<std::int64_t, kPriceDecimals>(str).transform(Price::from_raw);
}

static_assert(static_cast<std::uint64_t>(kPriceScale) == detail::pow10(kPriceDecimals),
              "kPriceScale must be 10^kPriceDecimals");

}  // namespace slipstream::core
