#include "codec/byte_io.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

// Expected API (src/codec/byte_io.hpp, namespace slipstream::codec):
//
//   template <class T> void store_le(std::span<std::byte> buf, std::size_t offset, T value) noexcept;
//   template <class T> T    load_le(std::span<const std::byte> buf, std::size_t offset) noexcept;
//
// T is a fixed-width integer or char. The caller guarantees offset + sizeof(T) <= buf.size().

namespace {

using slipstream::codec::load_le;
using slipstream::codec::store_le;

// Copies bytes into plain integers so a failing EXPECT_EQ prints readable values.
std::vector<unsigned> as_ints(std::span<const std::byte> bytes) {
    std::vector<unsigned> out;
    out.reserve(bytes.size());
    for (const std::byte b : bytes) {
        out.push_back(std::to_integer<unsigned>(b));
    }
    return out;
}

template <class... Ts>
constexpr std::array<std::byte, sizeof...(Ts)> make_bytes(Ts... values) {
    return {static_cast<std::byte>(values)...};
}

// ---- store_le: byte order ---------------------------------------------------------------------

TEST(StoreLe, U8IsASingleByte) {
    std::array<std::byte, 1> buf{};
    store_le<std::uint8_t>(buf, 0, 0xAB);
    EXPECT_EQ(as_ints(buf), (std::vector<unsigned>{0xAB}));
}

TEST(StoreLe, CharIsASingleByte) {
    std::array<std::byte, 1> buf{};
    store_le<char>(buf, 0, 'B');
    EXPECT_EQ(as_ints(buf), (std::vector<unsigned>{0x42}));
}

TEST(StoreLe, U16LowByteFirst) {
    std::array<std::byte, 2> buf{};
    store_le<std::uint16_t>(buf, 0, 0x0102);
    EXPECT_EQ(as_ints(buf), (std::vector<unsigned>{0x02, 0x01}));
}

TEST(StoreLe, U32LowByteFirst) {
    std::array<std::byte, 4> buf{};
    store_le<std::uint32_t>(buf, 0, 0x01020304);
    EXPECT_EQ(as_ints(buf), (std::vector<unsigned>{0x04, 0x03, 0x02, 0x01}));
}

TEST(StoreLe, U64LowByteFirst) {
    std::array<std::byte, 8> buf{};
    store_le<std::uint64_t>(buf, 0, 0x0102030405060708);
    EXPECT_EQ(as_ints(buf),
              (std::vector<unsigned>{0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01}));
}

// Negative numbers are two's complement: -2 is 0xFFFF'FFFF'FFFF'FFFE.
TEST(StoreLe, NegativeI64) {
    std::array<std::byte, 8> buf{};
    store_le<std::int64_t>(buf, 0, -2);
    EXPECT_EQ(as_ints(buf),
              (std::vector<unsigned>{0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}));
}

// ---- store_le: offsets ------------------------------------------------------------------------

TEST(StoreLe, WritesOnlyAtOffset) {
    std::array<std::byte, 8> buf{};
    buf.fill(std::byte{0xAA});
    store_le<std::uint16_t>(buf, 3, 0x0102);
    EXPECT_EQ(as_ints(buf),
              (std::vector<unsigned>{0xAA, 0xAA, 0xAA, 0x02, 0x01, 0xAA, 0xAA, 0xAA}));
}

// Wire fields sit at odd offsets (e.g. Trade id at 33). This must work regardless of alignment.
TEST(StoreLe, UnalignedOffset) {
    std::array<std::byte, 9> buf{};
    store_le<std::uint64_t>(buf, 1, 0x0102030405060708);
    EXPECT_EQ(as_ints(buf),
              (std::vector<unsigned>{0x00, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01}));
}

// ---- load_le ----------------------------------------------------------------------------------

TEST(LoadLe, U16FromKnownBytes) {
    constexpr auto buf = make_bytes(0x02, 0x01);
    EXPECT_EQ(load_le<std::uint16_t>(buf, 0), 0x0102u);
}

TEST(LoadLe, U32FromKnownBytes) {
    constexpr auto buf = make_bytes(0x04, 0x03, 0x02, 0x01);
    EXPECT_EQ(load_le<std::uint32_t>(buf, 0), 0x01020304u);
}

TEST(LoadLe, U64FromKnownBytes) {
    constexpr auto buf = make_bytes(0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01);
    EXPECT_EQ(load_le<std::uint64_t>(buf, 0), 0x0102030405060708u);
}

TEST(LoadLe, NegativeI64FromKnownBytes) {
    constexpr auto buf = make_bytes(0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    EXPECT_EQ(load_le<std::int64_t>(buf, 0), -2);
}

TEST(LoadLe, CharFromKnownByte) {
    constexpr auto buf = make_bytes(0x53);
    EXPECT_EQ(load_le<char>(buf, 0), 'S');
}

TEST(LoadLe, ReadsAtUnalignedOffset) {
    constexpr auto buf = make_bytes(0xAA, 0x04, 0x03, 0x02, 0x01, 0xAA);
    EXPECT_EQ(load_le<std::uint32_t>(buf, 1), 0x01020304u);
}

// ---- round trip -------------------------------------------------------------------------------

template <class T>
void expect_round_trip(T value) {
    std::array<std::byte, sizeof(T) + 3> buf{};
    store_le<T>(buf, 3, value);
    EXPECT_EQ(load_le<T>(buf, 3), value);
}

TEST(ByteIo, RoundTripsExtremes) {
    expect_round_trip<std::uint8_t>(0);
    expect_round_trip<std::uint8_t>(std::numeric_limits<std::uint8_t>::max());
    expect_round_trip<std::uint16_t>(std::numeric_limits<std::uint16_t>::max());
    expect_round_trip<std::uint32_t>(std::numeric_limits<std::uint32_t>::max());
    expect_round_trip<std::uint64_t>(std::numeric_limits<std::uint64_t>::max());
    expect_round_trip<std::int64_t>(std::numeric_limits<std::int64_t>::min());
    expect_round_trip<std::int64_t>(std::numeric_limits<std::int64_t>::max());
    expect_round_trip<std::int64_t>(0);
    expect_round_trip<char>('?');
}

}  // namespace
