#include "core/parse_fixed.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <expected>
#include <initializer_list>
#include <limits>
#include <ostream>
#include <string_view>

#include "support/alloc_guard.hpp"

namespace {

using slipstream::core::parse_fixed;
using slipstream::core::parse_price;
using slipstream::core::Price;
using slipstream::core::FixedParseError;

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

// ---------------------------------------------------------------------------------------------
// Valid inputs
// ---------------------------------------------------------------------------------------------

struct ValidCase {
    std::string_view input;
    std::int64_t raw;

    // Lets gtest print the case name on failure instead of a byte dump.
    friend std::ostream& operator<<(std::ostream& os, const ValidCase& c) {
        return os << '"' << c.input << '"';
    }
};

class ParsePriceValid : public ::testing::TestWithParam<ValidCase> {};

TEST_P(ParsePriceValid, ParsesToExpectedRaw) {
    const auto& [input, raw] = GetParam();
    const auto result = parse_price(input);
    ASSERT_TRUE(result.has_value()) << "input: \"" << input << '"';
    EXPECT_EQ(result->raw, raw) << "input: \"" << input << '"';
}

INSTANTIATE_TEST_SUITE_P(
    Ticket, ParsePriceValid,
    ::testing::Values(
        // Examples straight from issue #12.
        ValidCase{"101.25", 1'012'500},
        ValidCase{"0.5", 5'000},
        ValidCase{"1250", 12'500'000},
        ValidCase{"-3.1", -31'000},
        ValidCase{"0.0001", 1}));

INSTANTIATE_TEST_SUITE_P(
    FractionDigits, ParsePriceValid,
    ::testing::Values(
        // 0 to 4 fractional digits, each scaled up to 4 places.
        ValidCase{"7", 70'000},
        ValidCase{"7.1", 71'000},
        ValidCase{"7.12", 71'200},
        ValidCase{"7.123", 71'230},
        ValidCase{"7.1234", 71'234},
        // The fraction is positional: "0.05" is not "0.5".
        ValidCase{"0.05", 500},
        ValidCase{"0.005", 50},
        ValidCase{"1.0000", 10'000},
        // Leading zeros are harmless.
        ValidCase{"00012.50", 125'000},
        // Shapes from data/QandT.csv.
        ValidCase{"87.37", 873'700},
        ValidCase{"248.53", 2'485'300}));

INSTANTIATE_TEST_SUITE_P(
    Sign, ParsePriceValid,
    ::testing::Values(
        // The sign applies to the whole value, not just the integer part:
        // "-3.1" is -(3 * 10'000 + 1'000), not (-3 * 10'000) + 1'000.
        ValidCase{"-3.1", -31'000},
        ValidCase{"-101.25", -1'012'500},
        // The integer part is zero, so a parsed "-0" would lose the sign.
        ValidCase{"-0.5", -5'000},
        ValidCase{"-0.0001", -1},
        ValidCase{"0", 0},
        ValidCase{"-0", 0},
        ValidCase{"-0.0", 0}));

INSTANTIATE_TEST_SUITE_P(
    Limits, ParsePriceValid,
    ::testing::Values(
        // INT64_MAX / INT64_MIN are exactly representable.
        ValidCase{"922337203685477.5807", kMax},
        ValidCase{"-922337203685477.5807", -kMax},
        // INT64_MIN has no positive counterpart, so parsing |value| as int64 and negating
        // can't reach it.
        ValidCase{"-922337203685477.5808", kMin},
        // Largest integer part with no fraction.
        ValidCase{"922337203685477", 9'223'372'036'854'770'000},
        ValidCase{"922337203685477.58", 9'223'372'036'854'775'800}));

// ---------------------------------------------------------------------------------------------
// Invalid inputs
// ---------------------------------------------------------------------------------------------

struct InvalidCase {
    std::string_view input;
    FixedParseError error;

    friend std::ostream& operator<<(std::ostream& os, const InvalidCase& c) {
        return os << '"' << c.input << '"';
    }
};

class ParsePriceInvalid : public ::testing::TestWithParam<InvalidCase> {};

TEST_P(ParsePriceInvalid, ReturnsExpectedError) {
    const auto& [input, error] = GetParam();
    const auto result = parse_price(input);
    ASSERT_FALSE(result.has_value()) << "input: \"" << input << "\" parsed as raw " << result->raw;
    EXPECT_EQ(result.error(), error) << "input: \"" << input << '"';
}

INSTANTIATE_TEST_SUITE_P(
    Empty, ParsePriceInvalid,
    ::testing::Values(InvalidCase{"", FixedParseError::Empty}));

INSTANTIATE_TEST_SUITE_P(
    Format, ParsePriceInvalid,
    ::testing::Values(
        // Missing digits on one side of the dot, or entirely.
        InvalidCase{"-", FixedParseError::InvalidFormat},
        InvalidCase{".", FixedParseError::InvalidFormat},
        InvalidCase{"-.", FixedParseError::InvalidFormat},
        InvalidCase{"1.", FixedParseError::InvalidFormat},
        InvalidCase{".5", FixedParseError::InvalidFormat},
        InvalidCase{"-.5", FixedParseError::InvalidFormat},
        // Sign handling: only one leading '-', never '+', never inside the fraction.
        InvalidCase{"+1", FixedParseError::InvalidFormat},
        InvalidCase{"--1", FixedParseError::InvalidFormat},
        InvalidCase{"1-", FixedParseError::InvalidFormat},
        InvalidCase{"1.-5", FixedParseError::InvalidFormat},
        InvalidCase{"1.+5", FixedParseError::InvalidFormat},
        // More than one dot.
        InvalidCase{"1.2.3", FixedParseError::InvalidFormat},
        InvalidCase{"1..2", FixedParseError::InvalidFormat},
        // Trailing junk: std::from_chars stops at the first bad character and still reports
        // success, so the caller must check that it consumed everything.
        InvalidCase{"12abc", FixedParseError::InvalidFormat},
        InvalidCase{"1.5x", FixedParseError::InvalidFormat},
        // Whitespace is the CSV reader's job, not this function's.
        InvalidCase{" 1", FixedParseError::InvalidFormat},
        InvalidCase{"1 ", FixedParseError::InvalidFormat},
        InvalidCase{"1. 5", FixedParseError::InvalidFormat},
        // Other numeric spellings.
        InvalidCase{"1e3", FixedParseError::InvalidFormat},
        InvalidCase{"0x10", FixedParseError::InvalidFormat},
        InvalidCase{"1,5", FixedParseError::InvalidFormat},
        InvalidCase{"abc", FixedParseError::InvalidFormat},
        InvalidCase{"nan", FixedParseError::InvalidFormat}));

INSTANTIATE_TEST_SUITE_P(
    TooManyDecimals, ParsePriceInvalid,
    ::testing::Values(
        // Policy: reject rather than round (see issue #6). Counted by digits, so even
        // trailing zeros that would round to nothing are rejected.
        InvalidCase{"0.00001", FixedParseError::TooManyDecimals},
        InvalidCase{"1.23456", FixedParseError::TooManyDecimals},
        InvalidCase{"-1.23456", FixedParseError::TooManyDecimals},
        InvalidCase{"1.00000", FixedParseError::TooManyDecimals}));

INSTANTIATE_TEST_SUITE_P(
    OutOfRange, ParsePriceInvalid,
    ::testing::Values(
        // One tick past each limit.
        InvalidCase{"922337203685477.5808", FixedParseError::OutOfRange},
        InvalidCase{"-922337203685477.5809", FixedParseError::OutOfRange},
        // Integer part fits in int64, but times 10'000 it doesn't.
        InvalidCase{"922337203685478", FixedParseError::OutOfRange},
        InvalidCase{"1000000000000000", FixedParseError::OutOfRange},
        InvalidCase{"-922337203685478", FixedParseError::OutOfRange},
        // Integer part alone overflows int64 (from_chars reports this itself).
        InvalidCase{"99999999999999999999", FixedParseError::OutOfRange},
        InvalidCase{"-99999999999999999999", FixedParseError::OutOfRange},
        // Fits with a smaller fraction, overflows with this one.
        InvalidCase{"922337203685477.9999", FixedParseError::OutOfRange}));

// ---------------------------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------------------------

TEST(ParsePrice, IsUsableInConstantExpressions) {
    // Fails to compile if parse_price stops being constexpr-evaluable.
    constexpr auto result = parse_price("101.25");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->raw, 1'012'500);
}

TEST(ParsePrice, DoesNotAllocate) {
    std::int64_t sink = 0;
    slipstream::test::AllocGuard guard;
    for (const std::string_view s : {"101.25", "-0.5", "922337203685477.5807", "1.2.3", "1.23456"}) {
        const auto r = parse_price(s);
        sink ^= r ? r->raw : 1;
    }
    const auto allocations = guard.allocations();
    EXPECT_EQ(allocations, 0u);
    EXPECT_NE(sink, 0);
}

TEST(ParsePrice, ParsesOnlyTheGivenView) {
    // Parsing must respect the view's length, not run to a NUL. This is how the CSV reader
    // will call it: a view into the middle of a line.
    constexpr std::string_view line = "87.37,55";
    const auto result = parse_price(line.substr(0, 5));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->raw, 873'700);
}

// ---------------------------------------------------------------------------------------------
// parse_fixed<T, Decimals>: other types and precisions
//
// parse_price above covers the grammar thoroughly for <int64_t, 4>. These cases cover what
// changes with T and Decimals: scaling, the limits of T, and '-' on unsigned types.
// ---------------------------------------------------------------------------------------------

template <typename T>
struct FixedCase {
    std::string_view input;
    std::expected<T, FixedParseError> want;
};

template <typename T, unsigned Decimals>
void expect_parses(std::initializer_list<FixedCase<T>> cases) {
    for (const auto& [input, want] : cases) {
        SCOPED_TRACE(::testing::Message() << "input: \"" << input << '"');
        const auto got = parse_fixed<T, Decimals>(input);
        ASSERT_EQ(got.has_value(), want.has_value());
        if (want) {
            EXPECT_EQ(*got, *want);
        } else {
            EXPECT_EQ(got.error(), want.error());
        }
    }
}

constexpr auto err(FixedParseError e) { return std::unexpected{e}; }

TEST(ParseFixed, PartsPerMillion) {
    // --participation-cap: uint32_t, 6 decimals.
    using U = std::uint32_t;
    expect_parses<U, 6>({
        {"0.15", 150'000u},
        {"1", 1'000'000u},
        {"1.0", 1'000'000u},
        {"0", 0u},
        {"0.000001", 1u},
        {"0.123456", 123'456u},
        {"0.1234567", err(FixedParseError::TooManyDecimals)},
        // UINT32_MAX is 4'294'967'295.
        {"4294.967295", std::numeric_limits<U>::max()},
        {"4294.967296", err(FixedParseError::OutOfRange)},
        {"4295", err(FixedParseError::OutOfRange)},
    });
}

TEST(ParseFixed, TenthsOfABasisPoint) {
    // --band-bps: uint32_t, 1 decimal.
    using U = std::uint32_t;
    expect_parses<U, 1>({
        {"25.5", 255u},
        {"25", 250u},
        {"0.1", 1u},
        {"25.55", err(FixedParseError::TooManyDecimals)},
        {"429496729.5", std::numeric_limits<U>::max()},
        {"429496729.6", err(FixedParseError::OutOfRange)},
        {"429496730", err(FixedParseError::OutOfRange)},
    });
}

TEST(ParseFixed, UnsignedRejectsMinus) {
    // Even "-0": a sign on an unsigned value is a format error, not a range error.
    expect_parses<std::uint32_t, 6>({
        {"-0.15", err(FixedParseError::InvalidFormat)},
        {"-0", err(FixedParseError::InvalidFormat)},
        {"-", err(FixedParseError::InvalidFormat)},
    });
}

TEST(ParseFixed, SignedNarrowType) {
    // int32_t: the negative limit is one larger, as with int64_t. INT32_MIN is -2'147'483'648.
    using I = std::int32_t;
    expect_parses<I, 2>({
        {"-1.5", -150},
        {"21474836.47", std::numeric_limits<I>::max()},
        {"-21474836.48", std::numeric_limits<I>::min()},
        {"21474836.48", err(FixedParseError::OutOfRange)},
        {"-21474836.49", err(FixedParseError::OutOfRange)},
    });
}

TEST(ParseFixed, ZeroDecimalsIsPlainInteger) {
    expect_parses<std::int64_t, 0>({
        {"42", 42},
        {"-42", -42},
        {"4.2", err(FixedParseError::TooManyDecimals)},
        {"9223372036854775807", std::numeric_limits<std::int64_t>::max()},
        {"-9223372036854775808", std::numeric_limits<std::int64_t>::min()},
        {"9223372036854775808", err(FixedParseError::OutOfRange)},
    });
}

TEST(ParseFixed, Uint64UsesTheFullRange) {
    // No headroom above the limit here: the overflow check must not compute whole * 10 + digit
    // before comparing. UINT64_MAX is 18'446'744'073'709'551'615.
    using U = std::uint64_t;
    expect_parses<U, 0>({
        {"18446744073709551615", std::numeric_limits<U>::max()},
        {"18446744073709551616", err(FixedParseError::OutOfRange)},
        {"18446744073709551620", err(FixedParseError::OutOfRange)},
        {"99999999999999999999", err(FixedParseError::OutOfRange)},
    });
    // And with decimals: the fraction is what tips it over.
    expect_parses<U, 1>({
        {"1844674407370955161.5", std::numeric_limits<U>::max()},
        {"1844674407370955161.6", err(FixedParseError::OutOfRange)},
    });
}

TEST(ParseFixed, MaxDecimals) {
    // 18 decimals: the scale is 10^18, the largest power of ten that fits in uint64_t.
    using U = std::uint64_t;
    expect_parses<U, 18>({
        {"1", 1'000'000'000'000'000'000u},
        {"0.000000000000000001", 1u},
        {"18.446744073709551615", std::numeric_limits<U>::max()},
        {"18.446744073709551616", err(FixedParseError::OutOfRange)},
        {"19", err(FixedParseError::OutOfRange)},
    });
}

TEST(ParseFixed, IsUsableInConstantExpressions) {
    static_assert(parse_fixed<std::uint32_t, 6>("0.15") == 150'000u);
    static_assert(parse_fixed<std::uint32_t, 1>("25.5") == 255u);
    static_assert(!parse_fixed<std::uint32_t, 1>("-1").has_value());
}


}  // namespace
