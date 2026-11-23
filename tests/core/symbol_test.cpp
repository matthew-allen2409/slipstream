#include "core/symbol.hpp"

#include <gtest/gtest.h>

namespace {

using slipstream::core::Symbol;

TEST(Symbol, ShortNameIsNulPadded) {
    const auto s = Symbol::from_string("SYNTH1");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->view(), "SYNTH1");
    EXPECT_EQ(s->bytes()[6], '\0');
    EXPECT_EQ(s->bytes()[11], '\0');
}

// The spec's trap: a full 12-byte symbol has no terminator.
TEST(Symbol, FullTwelveByteNameHasNoTerminator) {
    const auto s = Symbol::from_string("ABCDEFGHIJKL");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->view(), "ABCDEFGHIJKL");
    EXPECT_EQ(s->view().size(), 12u);
}

TEST(Symbol, RejectsEmptyAndOversizedNames) {
    EXPECT_FALSE(Symbol::from_string("").has_value());
    EXPECT_FALSE(Symbol::from_string("ABCDEFGHIJKLM").has_value());
}

TEST(Symbol, EqualityComparesAllTwelveBytes) {
    EXPECT_EQ(Symbol::from_string("SYNTH1"), Symbol::from_string("SYNTH1"));
    EXPECT_NE(Symbol::from_string("SYNTH1"), Symbol::from_string("SYNTH2"));
    EXPECT_NE(Symbol::from_string("SYNTH1"), Symbol::from_string("SYNTH12"));
}

}  // namespace
