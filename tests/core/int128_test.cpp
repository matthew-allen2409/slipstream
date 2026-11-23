#include "core/int128.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace {

using slipstream::core::i128;

// The worst-case single px * qty term must fit with room for 2^32 more terms.
TEST(Int128, WorstCaseNotionalTermHasHeadroom) {
    constexpr i128 max_qty = std::numeric_limits<std::uint32_t>::max();
    constexpr i128 max_px = std::numeric_limits<std::int64_t>::max();
    constexpr i128 term = max_qty * max_px;  // < 2^95
    constexpr i128 bound = i128{1} << 95;
    static_assert(term < bound);
    EXPECT_LT(term, bound);
}

// Why int64 is not enough: a max-qty print at the data's highest price (1253.33) is about
// 5.4e16 raw, so 200 of them already exceed INT64_MAX (about 9.2e18).
TEST(Int128, Int64AccumulatorWouldOverflow) {
    constexpr i128 term = i128{std::numeric_limits<std::uint32_t>::max()} * 12'533'300;
    EXPECT_LT(term, i128{std::numeric_limits<std::int64_t>::max()});
    EXPECT_GT(term * 200, i128{std::numeric_limits<std::int64_t>::max()});
}

}  // namespace
