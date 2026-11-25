#pragma once

// Decoded, host-order message representations; the codec translates between these and the wire
// layout in wire.hpp. Field types must match the wire widths (checked in codec.cpp), and Price and
// Symbol are copied to the wire as their raw bytes.

#include <cstdint>

#include "core/fixed_point.hpp"
#include "core/symbol.hpp"

namespace slipstream::codec {

using core::Price;
using core::Qty;
using core::Symbol;

enum class Aggressor : char { Buy = 'B', Sell = 'S', Unknown = '?' };
enum class Side : char { Buy = 'B', Sell = 'S' };
enum class OrderStatus : char { Accepted = 'A', Rejected = 'R' };
enum class SessionState : std::uint8_t { Open = 0, Halt = 1, Close = 2 };
enum class ExecStatus : std::uint8_t { Ack = 0, Fill = 1, Partial = 2, Reject = 3 };

struct Quote {
    Symbol symbol;
    std::uint64_t ts_ns = 0;
    Qty bid_qty = 0;
    Price bid_px;
    Qty ask_qty = 0;
    Price ask_px;

    bool operator==(const Quote& other) const noexcept = default;
};

struct Trade {
    Symbol symbol;
    std::uint64_t ts_ns = 0;
    Qty qty = 0;
    Price px;
    Aggressor aggressor = Aggressor::Unknown;
    std::int64_t id = 0;

    bool operator==(const Trade& other) const noexcept = default;
};

struct Heartbeat {
    std::uint64_t ts_ns = 0;

    bool operator==(const Heartbeat& other) const noexcept = default;
};

struct SessionControl {
    std::uint64_t ts_ns = 0;
    SessionState state = SessionState::Open;

    bool operator==(const SessionControl& other) const noexcept = default;
};

struct NewOrder {
    std::uint64_t client_order_id = 0;
    Symbol symbol;
    OrderStatus status = OrderStatus::Rejected;
    std::uint64_t ts_ns = 0;
    std::int64_t trade_id = 0;
    Side side = Side::Buy;
    Qty qty = 0;
    Price limit_px;

    bool operator==(const NewOrder& other) const noexcept = default;
};

struct ExecReport {
    std::uint64_t client_order_id = 0;
    std::uint64_t ts_ns = 0;
    ExecStatus status = ExecStatus::Ack;
    Qty filled_qty = 0;
    Price avg_px;
    std::uint8_t reason_code = 0;

    bool operator==(const ExecReport& other) const noexcept = default;
};

}  // namespace slipstream::codec
