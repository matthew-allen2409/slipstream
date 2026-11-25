#include "codec/codec.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "codec/error.hpp"
#include "codec/messages.hpp"
#include "codec/wire.hpp"
#include "support/alloc_guard.hpp"

// Expected API (src/codec/codec.hpp, namespace slipstream::codec):
//
//   Encode writes header + body into `out` and returns the number of bytes written.
//   If `out` is too small it returns EncodeError::BufferTooSmall and writes nothing.
//
//     std::expected<std::size_t, EncodeError> encode(const Quote&,          std::span<std::byte>
//     out) noexcept; std::expected<std::size_t, EncodeError> encode(const Trade&,
//     std::span<std::byte> out) noexcept; std::expected<std::size_t, EncodeError> encode(const
//     Heartbeat&,      std::span<std::byte> out) noexcept; std::expected<std::size_t, EncodeError>
//     encode(const SessionControl&, std::span<std::byte> out) noexcept; std::expected<std::size_t,
//     EncodeError> encode(const NewOrder&,       std::span<std::byte> out) noexcept;
//     std::expected<std::size_t, EncodeError> encode(const ExecReport&,     std::span<std::byte>
//     out) noexcept;
//
//   Decode takes the BODY only (the header has already been read by whoever framed the message).
//   A body of the wrong length returns DecodeError::BadBodyLen; an out-of-range enum field
//   returns DecodeError::BadField.
//
//     std::expected<Quote,          DecodeError> decode_quote(std::span<const std::byte> body)
//     noexcept; std::expected<Trade,          DecodeError> decode_trade(std::span<const std::byte>
//     body) noexcept; std::expected<Heartbeat,      DecodeError> decode_heartbeat(std::span<const
//     std::byte> body) noexcept; std::expected<SessionControl, DecodeError>
//     decode_session_control(std::span<const std::byte> body) noexcept; std::expected<NewOrder,
//     DecodeError> decode_new_order(std::span<const std::byte> body) noexcept;
//     std::expected<ExecReport,     DecodeError> decode_exec_report(std::span<const std::byte>
//     body) noexcept;
//
// The round-trip tests compare whole messages with ==, so each struct in messages.hpp needs
//     friend bool operator==(const Quote&, const Quote&) = default;   // etc.
//
// The expected byte arrays below were generated independently with Python's struct module
// ("<" = little-endian, no padding), not by hand.

namespace {

using namespace slipstream::codec;
using slipstream::core::Price;

constexpr std::size_t kBufSize = 64;  // larger than any frame

Symbol sym(std::string_view name) {
    return Symbol::from_string(name).value();
}

std::vector<unsigned> as_ints(std::span<const std::byte> bytes) {
    std::vector<unsigned> out;
    out.reserve(bytes.size());
    for (const std::byte b : bytes) {
        out.push_back(std::to_integer<unsigned>(b));
    }
    return out;
}

std::vector<std::byte> to_bytes(std::span<const unsigned> ints) {
    std::vector<std::byte> out;
    out.reserve(ints.size());
    for (const unsigned v : ints) {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

// Encodes `msg` and checks the output is exactly `expected`, byte for byte.
template <class Msg>
void expect_encodes_to(const Msg& msg, std::span<const unsigned> expected) {
    std::array<std::byte, kBufSize> buf{};
    const auto written = encode(msg, buf);
    ASSERT_TRUE(written.has_value());
    ASSERT_EQ(*written, expected.size());
    EXPECT_EQ(as_ints(std::span<const std::byte>(buf).first(*written)),
              std::vector<unsigned>(expected.begin(), expected.end()));
}

// ---- reference messages (the same values the Python generator used) ---------------------------

Quote ref_quote() {
    return {.symbol = sym("SYNTH1"),
            .ts_ns = 1000,
            .bid_qty = 100,
            .bid_px = Price::from_raw(1'012'500),
            .ask_qty = 200,
            .ask_px = Price::from_raw(1'013'000)};
}

Trade ref_trade() {
    return {.symbol = sym("SYNTH1"),
            .ts_ns = 2000,
            .qty = 50,
            .px = Price::from_raw(1'012'600),
            .aggressor = Aggressor::Buy,
            .id = 7};
}

Heartbeat ref_heartbeat() {
    return {.ts_ns = 0x0102030405060708};
}

SessionControl ref_session_control() {
    return {.ts_ns = 3000, .state = SessionState::Halt};
}

NewOrder ref_new_order() {
    return {.client_order_id = 42,
            .symbol = sym("SYNTH1"),
            .status = OrderStatus::Accepted,
            .ts_ns = 4000,
            .trade_id = 7,
            .side = Side::Sell,
            .qty = 50,
            .limit_px = Price::from_raw(1'012'600)};
}

ExecReport ref_exec_report() {
    return {.client_order_id = 42,
            .ts_ns = 5000,
            .status = ExecStatus::Fill,
            .filled_qty = 50,
            .avg_px = Price::from_raw(1'012'600),
            .reason_code = 0};
}

// clang-format off
constexpr auto kQuoteBytes = std::to_array<unsigned>({
    0x2C, 0x00, 0x01, 0x01,                                                  // header: len 44, type 1, v1
    0x53, 0x59, 0x4E, 0x54, 0x48, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // "SYNTH1"
    0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                          // ts_ns   1000
    0x64, 0x00, 0x00, 0x00,                                                  // bid_qty 100
    0x14, 0x73, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,                          // bid_px  1012500
    0xC8, 0x00, 0x00, 0x00,                                                  // ask_qty 200
    0x08, 0x75, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00});                         // ask_px  1013000

constexpr auto kTradeBytes = std::to_array<unsigned>({
    0x29, 0x00, 0x02, 0x01,                                                  // header: len 41, type 2, v1
    0x53, 0x59, 0x4E, 0x54, 0x48, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // "SYNTH1"
    0xD0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                          // ts_ns 2000
    0x32, 0x00, 0x00, 0x00,                                                  // qty   50
    0x78, 0x73, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,                          // px    1012600
    0x42,                                                                    // aggressor 'B'
    0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});                         // id    7

constexpr auto kHeartbeatBytes = std::to_array<unsigned>({
    0x08, 0x00, 0x03, 0x01,                                                  // header: len 8, type 3, v1
    0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01});                         // ts_ns 0x0102030405060708

constexpr auto kSessionControlBytes = std::to_array<unsigned>({
    0x09, 0x00, 0x04, 0x01,                                                  // header: len 9, type 4, v1
    0xB8, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                          // ts_ns 3000
    0x01});                                                                   // state HALT

constexpr auto kNewOrderBytes = std::to_array<unsigned>({
    0x32, 0x00, 0x0A, 0x01,                                                  // header: len 50, type 10, v1
    0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                          // client_order_id 42
    0x53, 0x59, 0x4E, 0x54, 0x48, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // "SYNTH1"
    0x41,                                                                    // status 'A'
    0xA0, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                          // ts_ns 4000
    0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                          // trade_id 7
    0x53,                                                                    // side 'S'
    0x32, 0x00, 0x00, 0x00,                                                  // qty 50
    0x78, 0x73, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00});                         // limit_px 1012600

constexpr auto kExecReportBytes = std::to_array<unsigned>({
    0x1E, 0x00, 0x0B, 0x01,                                                  // header: len 30, type 11, v1
    0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                          // client_order_id 42
    0x88, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                          // ts_ns 5000
    0x01,                                                                    // status FILL
    0x32, 0x00, 0x00, 0x00,                                                  // filled_qty 50
    0x78, 0x73, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,                          // avg_px 1012600
    0x00});                                                                   // reason_code 0
// clang-format on

// ---- encode: exact bytes ----------------------------------------------------------------------

TEST(Encode, QuoteBytes) {
    expect_encodes_to(ref_quote(), kQuoteBytes);
}
TEST(Encode, TradeBytes) {
    expect_encodes_to(ref_trade(), kTradeBytes);
}
TEST(Encode, HeartbeatBytes) {
    expect_encodes_to(ref_heartbeat(), kHeartbeatBytes);
}
TEST(Encode, SessionControlBytes) {
    expect_encodes_to(ref_session_control(), kSessionControlBytes);
}
TEST(Encode, NewOrderBytes) {
    expect_encodes_to(ref_new_order(), kNewOrderBytes);
}
TEST(Encode, ExecReportBytes) {
    expect_encodes_to(ref_exec_report(), kExecReportBytes);
}

// ---- encode: buffer too small -----------------------------------------------------------------

// One byte short must fail, and must not leave a half-written frame behind.
template <class Msg>
void expect_rejects_short_buffer(const Msg& msg, std::size_t frame_size) {
    std::vector<std::byte> buf(frame_size - 1, std::byte{0xAA});
    const auto written = encode(msg, std::span<std::byte>(buf));
    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error(), EncodeError::BufferTooSmall);
    EXPECT_EQ(as_ints(buf), std::vector<unsigned>(frame_size - 1, 0xAA));
}

TEST(Encode, BufferTooSmallWritesNothing) {
    expect_rejects_short_buffer(ref_quote(), kQuoteBytes.size());
    expect_rejects_short_buffer(ref_trade(), kTradeBytes.size());
    expect_rejects_short_buffer(ref_heartbeat(), kHeartbeatBytes.size());
    expect_rejects_short_buffer(ref_session_control(), kSessionControlBytes.size());
    expect_rejects_short_buffer(ref_new_order(), kNewOrderBytes.size());
    expect_rejects_short_buffer(ref_exec_report(), kExecReportBytes.size());
}

TEST(Encode, EmptyBuffer) {
    const auto written = encode(ref_heartbeat(), std::span<std::byte>{});
    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error(), EncodeError::BufferTooSmall);
}

// Writing into the start of a larger buffer is fine; bytes after the frame stay untouched.
TEST(Encode, LeavesTrailingBytesAlone) {
    std::array<std::byte, kBufSize> buf{};
    buf.fill(std::byte{0xAA});
    const auto written = encode(ref_heartbeat(), buf);
    ASSERT_TRUE(written.has_value());
    for (std::size_t i = *written; i < buf.size(); ++i) {
        EXPECT_EQ(buf[i], std::byte{0xAA}) << "byte " << i;
    }
}

// ---- decode: from known bytes -----------------------------------------------------------------

// Strips the 4-byte header, since decoders take the body only.
std::vector<std::byte> body_of(std::span<const unsigned> frame) {
    return to_bytes(frame.subspan(header::kSize));
}

TEST(Decode, QuoteFromBytes) {
    const auto body = body_of(kQuoteBytes);
    const auto msg = decode_quote(body);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, ref_quote());
}

TEST(Decode, TradeFromBytes) {
    const auto body = body_of(kTradeBytes);
    const auto msg = decode_trade(body);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, ref_trade());
}

TEST(Decode, HeartbeatFromBytes) {
    const auto body = body_of(kHeartbeatBytes);
    const auto msg = decode_heartbeat(body);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, ref_heartbeat());
}

TEST(Decode, SessionControlFromBytes) {
    const auto body = body_of(kSessionControlBytes);
    const auto msg = decode_session_control(body);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, ref_session_control());
}

TEST(Decode, NewOrderFromBytes) {
    const auto body = body_of(kNewOrderBytes);
    const auto msg = decode_new_order(body);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, ref_new_order());
}

TEST(Decode, ExecReportFromBytes) {
    const auto body = body_of(kExecReportBytes);
    const auto msg = decode_exec_report(body);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, ref_exec_report());
}

// ---- decode: wrong body length ----------------------------------------------------------------

TEST(Decode, ShortBodyIsBadBodyLen) {
    auto body = body_of(kQuoteBytes);
    body.pop_back();
    const auto msg = decode_quote(body);
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(msg.error(), DecodeError::BadBodyLen);
}

TEST(Decode, LongBodyIsBadBodyLen) {
    auto body = body_of(kTradeBytes);
    body.push_back(std::byte{0});
    const auto msg = decode_trade(body);
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(msg.error(), DecodeError::BadBodyLen);
}

TEST(Decode, EmptyBodyIsBadBodyLen) {
    const auto msg = decode_heartbeat(std::span<const std::byte>{});
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(msg.error(), DecodeError::BadBodyLen);
}

// ---- decode: invalid field values -------------------------------------------------------------

// Takes a known-good body, overwrites one byte, and returns it.
std::vector<std::byte> corrupt(std::span<const unsigned> frame, std::size_t body_offset,
                               unsigned value) {
    auto body = body_of(frame);
    body.at(body_offset) = static_cast<std::byte>(value);
    return body;
}

TEST(Decode, TradeRejectsUnknownAggressor) {
    const auto msg = decode_trade(corrupt(kTradeBytes, trade::aggressor, 'X'));
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(msg.error(), DecodeError::BadField);
}

TEST(Decode, TradeAcceptsAllValidAggressors) {
    for (const char c : {'B', 'S', '?'}) {
        const auto msg =
            decode_trade(corrupt(kTradeBytes, trade::aggressor, static_cast<unsigned>(c)));
        ASSERT_TRUE(msg.has_value()) << "aggressor " << c;
        EXPECT_EQ(static_cast<char>(msg->aggressor), c);
    }
}

TEST(Decode, SessionControlRejectsStateAboveClose) {
    const auto msg =
        decode_session_control(corrupt(kSessionControlBytes, session_control::state, 3));
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(msg.error(), DecodeError::BadField);
}

TEST(Decode, NewOrderRejectsUnknownStatus) {
    const auto msg = decode_new_order(corrupt(kNewOrderBytes, new_order::status, 'X'));
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(msg.error(), DecodeError::BadField);
}

TEST(Decode, NewOrderRejectsUnknownSide) {
    const auto msg = decode_new_order(corrupt(kNewOrderBytes, new_order::side, '?'));
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(msg.error(), DecodeError::BadField);
}

TEST(Decode, ExecReportRejectsStatusAboveReject) {
    const auto msg = decode_exec_report(corrupt(kExecReportBytes, exec_report::status, 4));
    ASSERT_FALSE(msg.has_value());
    EXPECT_EQ(msg.error(), DecodeError::BadField);
}

// ---- round trip: decode(encode(x)) == x, including edge values -------------------------------

template <class Msg, class DecodeFn>
void expect_round_trip(const Msg& msg, DecodeFn decode) {
    std::array<std::byte, kBufSize> buf{};
    const auto written = encode(msg, buf);
    ASSERT_TRUE(written.has_value());
    const auto body =
        std::span<const std::byte>(buf).subspan(header::kSize, *written - header::kSize);
    const auto decoded = decode(body);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, msg);
}

TEST(RoundTrip, QuoteEdgeValues) {
    expect_round_trip(ref_quote(), decode_quote);
    expect_round_trip(Quote{}, decode_quote);  // all zeros, empty symbol bytes
    // Full 12-character symbol: no NUL terminator on the wire.
    expect_round_trip(Quote{.symbol = sym("ABCDEFGHIJKL"),
                            .ts_ns = std::numeric_limits<std::uint64_t>::max(),
                            .bid_qty = std::numeric_limits<std::uint32_t>::max(),
                            .bid_px = Price::from_raw(-1),
                            .ask_qty = std::numeric_limits<std::uint32_t>::max(),
                            .ask_px = Price::from_raw(std::numeric_limits<std::int64_t>::max())},
                      decode_quote);
    expect_round_trip(Quote{.symbol = sym("X"),
                            .bid_px = Price::from_raw(std::numeric_limits<std::int64_t>::min()),
                            .ask_px = Price::from_raw(0)},
                      decode_quote);
}

TEST(RoundTrip, TradeEdgeValues) {
    expect_round_trip(ref_trade(), decode_trade);
    expect_round_trip(Trade{.symbol = sym("ABCDEFGHIJKL"),
                            .ts_ns = std::numeric_limits<std::uint64_t>::max(),
                            .qty = std::numeric_limits<std::uint32_t>::max(),
                            .px = Price::from_raw(-12'345),
                            .aggressor = Aggressor::Unknown,
                            .id = std::numeric_limits<std::int64_t>::min()},
                      decode_trade);
    expect_round_trip(
        Trade{.symbol = sym("SYNTH1"), .px = Price::from_raw(0), .aggressor = Aggressor::Sell},
        decode_trade);
}

TEST(RoundTrip, HeartbeatEdgeValues) {
    expect_round_trip(Heartbeat{.ts_ns = 0}, decode_heartbeat);
    expect_round_trip(Heartbeat{.ts_ns = std::numeric_limits<std::uint64_t>::max()},
                      decode_heartbeat);
}

TEST(RoundTrip, SessionControlEveryState) {
    for (const auto s : {SessionState::Open, SessionState::Halt, SessionState::Close}) {
        expect_round_trip(SessionControl{.ts_ns = 123, .state = s}, decode_session_control);
    }
}

TEST(RoundTrip, NewOrderEdgeValues) {
    expect_round_trip(ref_new_order(), decode_new_order);
    expect_round_trip(
        NewOrder{.client_order_id = std::numeric_limits<std::uint64_t>::max(),
                 .symbol = sym("ABCDEFGHIJKL"),
                 .status = OrderStatus::Rejected,
                 .ts_ns = std::numeric_limits<std::uint64_t>::max(),
                 .trade_id = -1,
                 .side = Side::Buy,
                 .qty = std::numeric_limits<std::uint32_t>::max(),
                 .limit_px = Price::from_raw(std::numeric_limits<std::int64_t>::min())},
        decode_new_order);
}

TEST(RoundTrip, ExecReportEdgeValues) {
    expect_round_trip(ref_exec_report(), decode_exec_report);
    for (const auto s :
         {ExecStatus::Ack, ExecStatus::Fill, ExecStatus::Partial, ExecStatus::Reject}) {
        expect_round_trip(
            ExecReport{
                .client_order_id = 1, .status = s, .avg_px = Price::from_raw(0), .reason_code = 4},
            decode_exec_report);
    }
    expect_round_trip(ExecReport{.client_order_id = std::numeric_limits<std::uint64_t>::max(),
                                 .ts_ns = std::numeric_limits<std::uint64_t>::max(),
                                 .status = ExecStatus::Partial,
                                 .filled_qty = std::numeric_limits<std::uint32_t>::max(),
                                 .avg_px = Price::from_raw(-1),
                                 .reason_code = 255},
                      decode_exec_report);
}

// ---- exact-size buffers ------------------------------------------------------------------------

// A heap buffer of exactly the frame size: any write past the end is caught by AddressSanitizer
// (asan-ubsan preset). The 64-byte stack buffers used elsewhere would hide such a bug.
template <class Msg, class DecodeFn>
void expect_exact_fit(const Msg& msg, std::size_t frame_size, DecodeFn decode_fn) {
    std::vector<std::byte> buf(frame_size);
    const auto written = encode(msg, std::span<std::byte>(buf));
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, frame_size);
    const auto decoded = decode_fn(std::span<const std::byte>(buf).subspan(header::kSize));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, msg);
}

TEST(Encode, ExactSizeBufferFits) {
    expect_exact_fit(ref_quote(), kQuoteBytes.size(), decode_quote);
    expect_exact_fit(ref_trade(), kTradeBytes.size(), decode_trade);
    expect_exact_fit(ref_heartbeat(), kHeartbeatBytes.size(), decode_heartbeat);
    expect_exact_fit(ref_session_control(), kSessionControlBytes.size(), decode_session_control);
    expect_exact_fit(ref_new_order(), kNewOrderBytes.size(), decode_new_order);
    expect_exact_fit(ref_exec_report(), kExecReportBytes.size(), decode_exec_report);
}

// ---- symbol validation --------------------------------------------------------------------------

// "AB\0CD": would display as "AB" but compare unequal to Symbol "AB", so it is rejected.
TEST(Decode, RejectsNonCanonicalSymbolPadding) {
    // "SYNTH1" occupies symbol bytes 0..5 and bytes 6..11 are NUL padding. Byte 7 becomes 'X',
    // giving "SYNTH1\0X\0\0\0\0".
    constexpr std::size_t kPaddingByte = 7;

    const auto q = decode_quote(corrupt(kQuoteBytes, quote::symbol + kPaddingByte, 'X'));
    ASSERT_FALSE(q.has_value());
    EXPECT_EQ(q.error(), DecodeError::BadField);

    const auto t = decode_trade(corrupt(kTradeBytes, trade::symbol + kPaddingByte, 'X'));
    ASSERT_FALSE(t.has_value());
    EXPECT_EQ(t.error(), DecodeError::BadField);

    const auto o = decode_new_order(corrupt(kNewOrderBytes, new_order::symbol + kPaddingByte, 'X'));
    ASSERT_FALSE(o.has_value());
    EXPECT_EQ(o.error(), DecodeError::BadField);
}

// ---- decode<M> ----------------------------------------------------------------------------------

TEST(Decode, GenericDecodeMatchesNamedDecoders) {
    EXPECT_EQ(decode<Quote>(body_of(kQuoteBytes)), decode_quote(body_of(kQuoteBytes)));
    EXPECT_EQ(decode<Trade>(body_of(kTradeBytes)), decode_trade(body_of(kTradeBytes)));
    EXPECT_EQ(decode<Heartbeat>(body_of(kHeartbeatBytes)),
              decode_heartbeat(body_of(kHeartbeatBytes)));
    EXPECT_EQ(decode<SessionControl>(body_of(kSessionControlBytes)),
              decode_session_control(body_of(kSessionControlBytes)));
    EXPECT_EQ(decode<NewOrder>(body_of(kNewOrderBytes)), decode_new_order(body_of(kNewOrderBytes)));
    EXPECT_EQ(decode<ExecReport>(body_of(kExecReportBytes)),
              decode_exec_report(body_of(kExecReportBytes)));
}

// ---- no heap allocation -------------------------------------------------------------------------

// Encodes then decodes `msg`; returns true if both succeeded. Used inside an AllocGuard scope.
template <class Msg>
bool encode_decode(const Msg& msg, std::span<std::byte> buf) {
    const auto written = encode(msg, buf);
    if (!written) {
        return false;
    }
    const auto body =
        std::span<const std::byte>(buf).subspan(header::kSize, *written - header::kSize);
    return decode<Msg>(body).has_value();
}

// Encode and decode run on every message, so they must never touch the heap.
TEST(Codec, EncodeAndDecodeDoNotAllocate) {
    std::array<std::byte, kBufSize> buf{};
    const auto quote = ref_quote();
    const auto trade = ref_trade();
    const auto heartbeat = ref_heartbeat();
    const auto session = ref_session_control();
    const auto order = ref_new_order();
    const auto report = ref_exec_report();

    std::size_t allocations = 0;
    bool ok = true;
    {
        slipstream::test::AllocGuard guard;
        ok = encode_decode(quote, buf) && ok;
        ok = encode_decode(trade, buf) && ok;
        ok = encode_decode(heartbeat, buf) && ok;
        ok = encode_decode(session, buf) && ok;
        ok = encode_decode(order, buf) && ok;
        ok = encode_decode(report, buf) && ok;
        allocations = guard.allocations();
    }
    EXPECT_TRUE(ok);
    EXPECT_EQ(allocations, 0u);
}

}  // namespace
