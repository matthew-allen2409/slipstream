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

TEST(Symbol, RejectsEmbeddedNulInName) {
    using namespace std::string_view_literals;
    EXPECT_FALSE(Symbol::from_string("AB\0CD"sv).has_value());
}

TEST(Symbol, FromBytesAcceptsCanonicalPadding) {
    Symbol::Storage bytes{'S', 'Y', 'N', 'T', 'H', '1'};  // rest zero-filled
    const auto s = Symbol::from_bytes(bytes);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s, Symbol::from_string("SYNTH1"));
}

TEST(Symbol, FromBytesAcceptsAllPaddingAndFullName) {
    EXPECT_TRUE(Symbol::from_bytes(Symbol::Storage{}).has_value());
    const Symbol::Storage full{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L'};
    EXPECT_TRUE(Symbol::from_bytes(full).has_value());
}

// "AB\0CD": displays as "AB" but would compare unequal to Symbol "AB".
TEST(Symbol, FromBytesRejectsDataAfterPadding) {
    const Symbol::Storage bytes{'A', 'B', '\0', 'C', 'D'};
    EXPECT_FALSE(Symbol::from_bytes(bytes).has_value());
}

}  // namespace
