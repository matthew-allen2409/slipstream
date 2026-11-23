#include "core/fixed_point.hpp"

#include <gtest/gtest.h>

namespace {

using slipstream::core::Price;

TEST(FixedPoint, ScaleIsTenThousand) {
    EXPECT_EQ(Price::from_raw(1'012'500).raw, 1'012'500);
    EXPECT_DOUBLE_EQ(slipstream::core::to_display(Price::from_raw(1'012'500)), 101.25);
}

TEST(FixedPoint, OrdersByRawValue) {
    EXPECT_LT(Price::from_raw(1'012'400), Price::from_raw(1'012'500));
    EXPECT_EQ(Price::from_raw(0), Price{});
}

}  // namespace
