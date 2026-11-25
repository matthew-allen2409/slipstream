#pragma once

#include <cstddef>
#include <cstdint>

#include "core/symbol.hpp"

namespace slipstream::codec {

inline constexpr std::uint8_t kProtocolVersion = 1;

enum class MsgType : std::uint8_t {
    Quote = 1,
    Trade = 2,
    Heartbeat = 3,
    SessionControl = 4,
    NewOrder = 10,
    ExecReport = 11,
};

namespace header {
inline constexpr std::size_t body_len = 0;  // uint16
inline constexpr std::size_t msg_type = 2;  // uint8
inline constexpr std::size_t version = 3;   // uint8
inline constexpr std::size_t kSize = 4;
}

namespace quote {
inline constexpr std::size_t symbol = 0;    // char[12]
inline constexpr std::size_t ts_ns = 12;    // uint64
inline constexpr std::size_t bid_qty = 20;  // uint32
inline constexpr std::size_t bid_px = 24;   // int64
inline constexpr std::size_t ask_qty = 32;  // uint32
inline constexpr std::size_t ask_px = 36;   // int64
inline constexpr std::size_t kSize = 44;
static_assert(ts_ns == symbol + core::Symbol::kSize);
static_assert(ask_px + sizeof(std::int64_t) == kSize);
}  // namespace quote

namespace trade {
inline constexpr std::size_t symbol = 0;     // char[12]
inline constexpr std::size_t ts_ns = 12;     // uint64
inline constexpr std::size_t qty = 20;       // uint32
inline constexpr std::size_t px = 24;        // int64
inline constexpr std::size_t aggressor = 32; // char: 'B', 'S', '?'
inline constexpr std::size_t id = 33;        // int64
inline constexpr std::size_t kSize = 41;     // spec text says 49; field table sums to 41
static_assert(ts_ns == symbol + core::Symbol::kSize);
static_assert(id + sizeof(std::int64_t) == kSize);
}  // namespace trade

namespace heartbeat {
inline constexpr std::size_t ts_ns = 0;  // uint64
inline constexpr std::size_t kSize = 8;
static_assert(ts_ns + sizeof(std::uint64_t) == kSize);
}  // namespace heartbeat

namespace session_control {
inline constexpr std::size_t ts_ns = 0;  // uint64
inline constexpr std::size_t state = 8;  // uint8: 0=OPEN, 1=HALT, 2=CLOSE
inline constexpr std::size_t kSize = 9;
static_assert(state + sizeof(std::uint8_t) == kSize);
}  // namespace session_control

namespace new_order {
inline constexpr std::size_t client_order_id = 0;  // uint64
inline constexpr std::size_t symbol = 8;           // char[12]
inline constexpr std::size_t status = 20;          // char: 'A' / 'R'
inline constexpr std::size_t ts_ns = 21;           // uint64
inline constexpr std::size_t trade_id = 29;        // int64
inline constexpr std::size_t side = 37;            // char: 'B' / 'S'
inline constexpr std::size_t qty = 38;             // uint32
inline constexpr std::size_t limit_px = 42;        // int64
inline constexpr std::size_t kSize = 50;           // spec text says 42; field table sums to 50
static_assert(status == symbol + core::Symbol::kSize);
static_assert(limit_px + sizeof(std::int64_t) == kSize);
}  // namespace new_order

namespace exec_report {
inline constexpr std::size_t client_order_id = 0;  // uint64
inline constexpr std::size_t ts_ns = 8;            // uint64
inline constexpr std::size_t status = 16;          // uint8
inline constexpr std::size_t filled_qty = 17;      // uint32
inline constexpr std::size_t avg_px = 21;          // int64
inline constexpr std::size_t reason_code = 29;     // uint8
inline constexpr std::size_t kSize = 30;
static_assert(reason_code + sizeof(std::uint8_t) == kSize);
}  // namespace exec_report

// Largest body of any known message; sizes the parser's scratch space.
inline constexpr std::size_t kMaxBodySize = new_order::kSize;

// Exact body length for a message type, or 0 if the type is unknown.
[[nodiscard]] constexpr std::size_t expected_body_size(MsgType type) noexcept {
    switch (type) {
        case MsgType::Quote: return quote::kSize;
        case MsgType::Trade: return trade::kSize;
        case MsgType::Heartbeat: return heartbeat::kSize;
        case MsgType::SessionControl: return session_control::kSize;
        case MsgType::NewOrder: return new_order::kSize;
        case MsgType::ExecReport: return exec_report::kSize;
    }
    return 0;
}

}  // namespace slipstream::codec
