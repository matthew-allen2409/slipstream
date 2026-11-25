#include "codec/frame_parser.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "codec/codec.hpp"
#include "codec/error.hpp"
#include "codec/messages.hpp"
#include "codec/wire.hpp"
#include "support/alloc_guard.hpp"

// Expected API (src/codec/frame_parser.hpp, namespace slipstream::codec):
//
//   struct Frame {
//       MsgType type;
//       std::span<const std::byte> body;   // a view into the parser's buffer, not a copy
//   };
//
//   class FrameParser {
//   public:
//       static constexpr std::size_t kCapacity = 4096;
//
//       // Copies as much of `data` as fits into the internal buffer and returns how many bytes
//       // were accepted. Accepts fewer than data.size() only when the buffer is full; the caller
//       // then drains frames with next() and feeds the remainder.
//       std::size_t feed(std::span<const std::byte> data) noexcept;
//
//       // Returns:
//       //   a Frame        a complete frame whose header has been validated. Its body view is
//       //                  valid until the next call to feed() or next().
//       //   std::nullopt   not enough bytes yet for a whole frame (NOT an error)
//       //   a DecodeError  BadVersion, UnknownType or BadBodyLen, reported as soon as the 4-byte
//       //                  header has arrived, without waiting for the body.
//       // A TCP stream cannot be resynchronised after a bad header, so once next() has returned
//       // an error it returns that same error forever. The caller should close the connection.
//       std::expected<std::optional<Frame>, DecodeError> next() noexcept;
//
//       // Bytes accepted by feed() and not yet returned as part of a frame.
//       std::size_t buffered() const noexcept;
//   };

namespace {

using namespace slipstream::codec;
using slipstream::core::Price;

// ---- building byte streams -------------------------------------------------------------------
// Frames are produced with the codec's encode(), which codec_test.cpp verifies byte for byte.

using Bytes = std::vector<std::byte>;

Symbol sym(std::string_view name) {
    return Symbol::from_string(name).value_or(Symbol{});
}

template <class Msg>
Bytes frame_of(const Msg& msg) {
    std::array<std::byte, 64> buf{};
    const auto written = encode(msg, buf);
    EXPECT_TRUE(written.has_value());
    return {buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(written.value_or(0))};
}

Bytes concat(const std::vector<Bytes>& frames) {
    Bytes out;
    for (const auto& f : frames) {
        out.insert(out.end(), f.begin(), f.end());
    }
    return out;
}

// One frame of every message type, in wire order.
std::vector<Bytes> one_of_each() {
    return {
        frame_of(Quote{.symbol = sym("SYNTH1"),
                       .ts_ns = 1000,
                       .bid_qty = 100,
                       .bid_px = Price::from_raw(1'012'500),
                       .ask_qty = 200,
                       .ask_px = Price::from_raw(1'013'000)}),
        frame_of(Trade{.symbol = sym("SYNTH1"),
                       .ts_ns = 2000,
                       .qty = 50,
                       .px = Price::from_raw(1'012'600),
                       .aggressor = Aggressor::Buy,
                       .id = 7}),
        frame_of(Heartbeat{.ts_ns = 3000}),
        frame_of(SessionControl{.ts_ns = 4000, .state = SessionState::Halt}),
        frame_of(NewOrder{.client_order_id = 42,
                          .symbol = sym("SYNTH1"),
                          .status = OrderStatus::Accepted,
                          .ts_ns = 5000,
                          .trade_id = 7,
                          .side = Side::Sell,
                          .qty = 50,
                          .limit_px = Price::from_raw(1'012'600)}),
        frame_of(ExecReport{.client_order_id = 42,
                            .ts_ns = 6000,
                            .status = ExecStatus::Fill,
                            .filled_qty = 50,
                            .avg_px = Price::from_raw(1'012'600),
                            .reason_code = 0}),
    };
}

// `count` frames cycling through every type, with fields that differ per frame so that a frame
// returned twice, skipped or reordered is detected.
std::vector<Bytes> mixed_frames(std::size_t count) {
    std::vector<Bytes> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto n32 = static_cast<std::uint32_t>(i);
        const auto n64 = static_cast<std::int64_t>(i);
        switch (i % 6) {
            case 0:
                out.push_back(frame_of(Quote{.symbol = sym("SYNTH1"),
                                             .ts_ns = i,
                                             .bid_qty = n32,
                                             .bid_px = Price::from_raw(n64),
                                             .ask_qty = n32 + 1,
                                             .ask_px = Price::from_raw(n64 + 1)}));
                break;
            case 1:
                out.push_back(frame_of(Trade{.symbol = sym("SYNTH2"),
                                             .ts_ns = i,
                                             .qty = n32,
                                             .px = Price::from_raw(-n64),
                                             .aggressor = Aggressor::Sell,
                                             .id = n64}));
                break;
            case 2:
                out.push_back(frame_of(Heartbeat{.ts_ns = i}));
                break;
            case 3:
                out.push_back(frame_of(SessionControl{.ts_ns = i, .state = SessionState::Open}));
                break;
            case 4:
                out.push_back(frame_of(NewOrder{.client_order_id = i,
                                                .symbol = sym("ABCDEFGHIJKL"),
                                                .status = OrderStatus::Rejected,
                                                .ts_ns = i,
                                                .trade_id = n64,
                                                .side = Side::Buy,
                                                .qty = n32,
                                                .limit_px = Price::from_raw(n64)}));
                break;
            default:
                out.push_back(frame_of(ExecReport{.client_order_id = i,
                                                  .ts_ns = i,
                                                  .status = ExecStatus::Ack,
                                                  .filled_qty = n32,
                                                  .avg_px = Price::from_raw(n64),
                                                  .reason_code = 1}));
                break;
        }
    }
    return out;
}

Bytes header_bytes(std::uint16_t body_len, std::uint8_t type, std::uint8_t version) {
    return {static_cast<std::byte>(body_len & 0xFFu), static_cast<std::byte>(body_len >> 8u),
            static_cast<std::byte>(type), static_cast<std::byte>(version)};
}

// ---- reading frames back ---------------------------------------------------------------------

// A copy of one parsed frame. The parser's body view only lives until the next call, so tests
// copy it before moving on.
struct Parsed {
    MsgType type{};
    Bytes body;
    bool operator==(const Parsed&) const = default;
};

// What the parser should return for an encoded frame, worked out from the bytes directly.
Parsed expected_frame(const Bytes& frame) {
    return {static_cast<MsgType>(frame.at(header::msg_type)),
            Bytes(frame.begin() + static_cast<std::ptrdiff_t>(header::kSize), frame.end())};
}

std::vector<Parsed> expected_frames(const std::vector<Bytes>& frames) {
    std::vector<Parsed> out;
    for (const auto& f : frames) {
        out.push_back(expected_frame(f));
    }
    return out;
}

// Calls next() until it reports "need more bytes", copying out each frame.
std::vector<Parsed> drain(FrameParser& parser) {
    std::vector<Parsed> out;
    while (true) {
        const auto result = parser.next();
        if (!result.has_value()) {
            ADD_FAILURE() << "unexpected decode error: " << to_string(result.error());
            return out;
        }
        if (!result->has_value()) {
            return out;
        }
        const Frame& frame = **result;
        out.push_back({frame.type, Bytes(frame.body.begin(), frame.body.end())});
    }
}

// Feeds all of `data`, draining whenever the buffer fills, the way a recv() loop would.
std::vector<Parsed> feed_all(FrameParser& parser, std::span<const std::byte> data) {
    std::vector<Parsed> out;
    while (!data.empty()) {
        const std::size_t accepted = parser.feed(data);
        data = data.subspan(accepted);
        auto frames = drain(parser);
        if (accepted == 0 && frames.empty()) {
            ADD_FAILURE() << "parser stalled: it accepts no bytes and returns no frames";
            return out;
        }
        out.insert(out.end(), frames.begin(), frames.end());
    }
    return out;
}

// Feeds `bytes` to a fresh parser and returns the error next() reports, if any.
std::optional<DecodeError> error_for(const Bytes& bytes) {
    FrameParser parser;
    EXPECT_EQ(parser.feed(bytes), bytes.size());
    const auto result = parser.next();
    if (result.has_value()) {
        return std::nullopt;
    }
    return result.error();
}

// ---- basics ----------------------------------------------------------------------------------

TEST(FrameParser, EmptyParserNeedsMoreBytes) {
    FrameParser parser;
    const auto result = parser.next();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
    EXPECT_EQ(parser.buffered(), 0u);
}

TEST(FrameParser, HeaderAloneIsNotAFrame) {
    const auto frame = one_of_each().front();
    FrameParser parser;
    ASSERT_EQ(parser.feed(std::span(frame).first(header::kSize)), header::kSize);
    const auto result = parser.next();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
    EXPECT_EQ(parser.buffered(), header::kSize);
}

// Fewer than 4 bytes cannot be judged at all, even if they look wrong: wait for the rest.
TEST(FrameParser, PartialHeaderIsNotAnError) {
    const auto bad = header_bytes(1, 99, 7);
    FrameParser parser;
    ASSERT_EQ(parser.feed(std::span(bad).first(3)), 3u);
    const auto result = parser.next();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());
}

// Each type comes out with the right MsgType, and its body decodes back to the original.
template <class Msg>
void expect_round_trip_through_parser(const Msg& msg, MsgType type) {
    const auto bytes = frame_of(msg);
    FrameParser parser;
    ASSERT_EQ(parser.feed(bytes), bytes.size());

    const auto result = parser.next();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value());
    const Frame& frame = **result;
    EXPECT_EQ(frame.type, type);
    EXPECT_EQ(frame.body.size(), bytes.size() - header::kSize);
    const auto decoded = decode<Msg>(frame.body);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, msg);

    const auto after = parser.next();
    ASSERT_TRUE(after.has_value());
    EXPECT_FALSE(after->has_value());
    EXPECT_EQ(parser.buffered(), 0u);
}

TEST(FrameParser, EveryMessageTypeParsesAndDecodes) {
    expect_round_trip_through_parser(Quote{.symbol = sym("SYNTH1"),
                                           .ts_ns = 1,
                                           .bid_qty = 2,
                                           .bid_px = Price::from_raw(3),
                                           .ask_qty = 4,
                                           .ask_px = Price::from_raw(5)},
                                     MsgType::Quote);
    expect_round_trip_through_parser(Trade{.symbol = sym("SYNTH1"),
                                           .ts_ns = 1,
                                           .qty = 2,
                                           .px = Price::from_raw(3),
                                           .aggressor = Aggressor::Unknown,
                                           .id = 4},
                                     MsgType::Trade);
    expect_round_trip_through_parser(Heartbeat{.ts_ns = 1}, MsgType::Heartbeat);
    expect_round_trip_through_parser(SessionControl{.ts_ns = 1, .state = SessionState::Close},
                                     MsgType::SessionControl);
    expect_round_trip_through_parser(NewOrder{.client_order_id = 1,
                                              .symbol = sym("SYNTH1"),
                                              .status = OrderStatus::Accepted,
                                              .ts_ns = 2,
                                              .trade_id = 3,
                                              .side = Side::Sell,
                                              .qty = 4,
                                              .limit_px = Price::from_raw(5)},
                                     MsgType::NewOrder);
    expect_round_trip_through_parser(ExecReport{.client_order_id = 1,
                                                .ts_ns = 2,
                                                .status = ExecStatus::Partial,
                                                .filled_qty = 3,
                                                .avg_px = Price::from_raw(4),
                                                .reason_code = 2},
                                     MsgType::ExecReport);
}

// ---- frames split across feed() calls (the spec's recv()-boundary requirement) ---------------

TEST(FrameParser, FrameSplitAtEveryByteBoundary) {
    for (const auto& frame : one_of_each()) {
        SCOPED_TRACE("msg_type " +
                     std::to_string(std::to_integer<int>(frame.at(header::msg_type))));
        const std::span<const std::byte> bytes(frame);

        for (std::size_t split = 1; split < frame.size(); ++split) {
            FrameParser parser;
            ASSERT_EQ(parser.feed(bytes.first(split)), split);

            const auto early = parser.next();
            ASSERT_TRUE(early.has_value()) << "split at " << split;
            EXPECT_FALSE(early->has_value())
                << "returned a frame after only " << split << " of " << frame.size() << " bytes";

            ASSERT_EQ(parser.feed(bytes.subspan(split)), frame.size() - split);
            const auto frames = drain(parser);
            ASSERT_EQ(frames.size(), 1u) << "split at " << split;
            EXPECT_EQ(frames[0], expected_frame(frame)) << "split at " << split;
        }
    }
}

TEST(FrameParser, ByteAtATime) {
    const auto frames = one_of_each();
    const auto stream = concat(frames);

    FrameParser parser;
    std::vector<Parsed> got;
    for (const std::byte& b : stream) {
        ASSERT_EQ(parser.feed(std::span(&b, 1)), 1u);
        const auto out = drain(parser);
        got.insert(got.end(), out.begin(), out.end());
    }
    EXPECT_EQ(got, expected_frames(frames));
    EXPECT_EQ(parser.buffered(), 0u);
}

TEST(FrameParser, SeveralFramesInOneFeedWithTrailingPartial) {
    const auto frames = one_of_each();
    const Bytes complete = concat({frames[0], frames[1], frames[2]});
    const Bytes& partial = frames[3];
    const std::size_t half = partial.size() / 2;

    Bytes first_chunk = complete;
    first_chunk.insert(first_chunk.end(), partial.begin(),
                       partial.begin() + static_cast<std::ptrdiff_t>(half));

    FrameParser parser;
    ASSERT_EQ(parser.feed(first_chunk), first_chunk.size());
    EXPECT_EQ(drain(parser), expected_frames({frames[0], frames[1], frames[2]}));
    EXPECT_EQ(parser.buffered(), half);

    ASSERT_EQ(parser.feed(std::span(partial).subspan(half)), partial.size() - half);
    EXPECT_EQ(drain(parser), expected_frames({partial}));
    EXPECT_EQ(parser.buffered(), 0u);
}

// A long stream (several times the buffer size) in random-sized chunks must come out identical to
// the frames that went in. Passing this needs the buffer to reuse space (compaction).
TEST(FrameParser, RandomChunkingMatchesWholeStream) {
    const auto frames = mixed_frames(600);
    const auto stream = concat(frames);
    ASSERT_GT(stream.size(), 4 * FrameParser::kCapacity);

    std::mt19937 rng(20261124);  // fixed seed: failures are reproducible
    std::uniform_int_distribution<std::size_t> chunk_size(1, 300);

    FrameParser parser;
    std::vector<Parsed> got;
    std::span<const std::byte> rest(stream);
    while (!rest.empty()) {
        const std::size_t n = std::min(chunk_size(rng), rest.size());
        const auto out = feed_all(parser, rest.first(n));
        got.insert(got.end(), out.begin(), out.end());
        rest = rest.subspan(n);
    }

    ASSERT_EQ(got.size(), frames.size());
    EXPECT_EQ(got, expected_frames(frames));
    EXPECT_EQ(parser.buffered(), 0u);
}

// ---- a full buffer ---------------------------------------------------------------------------

TEST(FrameParser, FeedStopsWhenBufferIsFull) {
    const auto frames = mixed_frames(300);
    const auto stream = concat(frames);
    ASSERT_GT(stream.size(), 2 * FrameParser::kCapacity);

    FrameParser parser;
    const std::size_t accepted = parser.feed(stream);
    EXPECT_EQ(accepted, FrameParser::kCapacity) << "feed() should fill the buffer completely";
    EXPECT_EQ(parser.buffered(), FrameParser::kCapacity);
    EXPECT_EQ(parser.feed(std::span(stream).subspan(accepted)), 0u)
        << "a full buffer accepts nothing more until frames are drained";

    // Draining frees space; everything still comes out, in order.
    auto got = drain(parser);
    const auto rest = feed_all(parser, std::span(stream).subspan(accepted));
    got.insert(got.end(), rest.begin(), rest.end());
    EXPECT_EQ(got, expected_frames(frames));
}

// ---- bad headers -----------------------------------------------------------------------------

TEST(FrameParser, RejectsWrongVersion) {
    EXPECT_EQ(error_for(header_bytes(quote::kSize, 1, 0)), DecodeError::BadVersion);
    EXPECT_EQ(error_for(header_bytes(quote::kSize, 1, 2)), DecodeError::BadVersion);
}

TEST(FrameParser, RejectsUnknownMessageType) {
    EXPECT_EQ(error_for(header_bytes(8, 0, 1)), DecodeError::UnknownType);
    EXPECT_EQ(error_for(header_bytes(8, 5, 1)), DecodeError::UnknownType);  // gap between 4 and 10
    EXPECT_EQ(error_for(header_bytes(8, 99, 1)), DecodeError::UnknownType);
}

TEST(FrameParser, RejectsBodyLengthThatDoesNotMatchType) {
    EXPECT_EQ(error_for(header_bytes(quote::kSize - 1, 1, 1)), DecodeError::BadBodyLen);
    EXPECT_EQ(error_for(header_bytes(0, 3, 1)), DecodeError::BadBodyLen);
    EXPECT_EQ(error_for(header_bytes(trade::kSize + 8, 2, 1)), DecodeError::BadBodyLen);  // 49
}

// A huge body_len must be rejected from the header alone. If the parser waited for 65535 bytes
// it would hang, and the body could never fit in a 4 KiB buffer anyway.
TEST(FrameParser, RejectsHugeBodyLengthWithoutWaitingForBody) {
    EXPECT_EQ(error_for(header_bytes(0xFFFF, 1, 1)), DecodeError::BadBodyLen);
}

TEST(FrameParser, ErrorIsSticky) {
    FrameParser parser;
    const auto bad = header_bytes(8, 99, 1);
    ASSERT_EQ(parser.feed(bad), bad.size());

    const auto first = parser.next();
    ASSERT_FALSE(first.has_value());
    const auto second = parser.next();
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), first.error());

    // Even valid bytes arriving afterwards must not "recover" the stream: there is no way to know
    // where the next real frame starts. (Whether feed() accepts them is up to the parser.)
    const auto good = one_of_each().front();
    static_cast<void>(parser.feed(good));
    const auto third = parser.next();
    ASSERT_FALSE(third.has_value()) << "parser resumed after a bad header";
    EXPECT_EQ(third.error(), first.error());
}

TEST(FrameParser, FramesBeforeABadHeaderAreStillDelivered) {
    const auto frames = one_of_each();
    Bytes stream = concat({frames[0], frames[1]});
    const auto bad = header_bytes(8, 99, 1);
    stream.insert(stream.end(), bad.begin(), bad.end());

    FrameParser parser;
    ASSERT_EQ(parser.feed(stream), stream.size());

    for (const auto& expected : {frames[0], frames[1]}) {
        const auto result = parser.next();
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->has_value());
        EXPECT_EQ(((**result).type), expected_frame(expected).type);
    }
    const auto result = parser.next();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DecodeError::UnknownType);
}

// ---- no heap allocation ----------------------------------------------------------------------

TEST(FrameParser, DoesNotAllocate) {
    const auto frames = mixed_frames(600);
    const auto stream = concat(frames);

    // No gtest assertions inside the guarded scope: their failure messages allocate. Problems are
    // recorded in flags and checked afterwards.
    std::size_t count = 0;
    bool error = false;
    bool stalled = false;
    std::size_t allocations = 0;
    {
        slipstream::test::AllocGuard guard;
        FrameParser parser;
        std::span<const std::byte> rest(stream);
        while (!rest.empty() && !error && !stalled) {
            const std::size_t accepted =
                parser.feed(rest.first(std::min<std::size_t>(97, rest.size())));
            rest = rest.subspan(accepted);
            std::size_t got = 0;
            while (true) {
                const auto result = parser.next();
                if (!result.has_value()) {
                    error = true;
                    break;
                }
                if (!result->has_value()) {
                    break;
                }
                ++got;
            }
            count += got;
            stalled = accepted == 0 && got == 0;  // a broken parser must fail, not loop forever
        }
        allocations = guard.allocations();
    }
    EXPECT_FALSE(error);
    EXPECT_FALSE(stalled) << "parser stopped accepting bytes without returning frames";
    EXPECT_EQ(count, frames.size());
    EXPECT_EQ(allocations, 0u);
}

}  // namespace
