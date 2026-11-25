#pragma once

/// @file
/// @brief Decoded, host-order message representations.
///
/// The codec translates between these and the wire layout in wire.hpp. Field types must match the
/// wire widths (checked in codec.cpp), and Price and Symbol are copied to the wire as their raw
/// bytes.

#include <cstdint>

#include "core/fixed_point.hpp"
#include "core/symbol.hpp"

namespace slipstream::codec {

using core::Price;
using core::Qty;
using core::Symbol;

/// @brief Which side initiated a trade.
enum class Aggressor : char { Buy = 'B', Sell = 'S', Unknown = '?' };
/// @brief Order side.
enum class Side : char { Buy = 'B', Sell = 'S' };
/// @brief Whether the engine accepted or rejected a trade (carried in NewOrder).
enum class OrderStatus : char { Accepted = 'A', Rejected = 'R' };
/// @brief Trading session state.
enum class SessionState : std::uint8_t { Open = 0, Halt = 1, Close = 2 };
/// @brief Execution report status.
enum class ExecStatus : std::uint8_t { Ack = 0, Fill = 1, Partial = 2, Reject = 3 };

/// @brief GCMD/1 top-of-book (L1) quote.
struct Quote {
    Symbol symbol;            ///< Instrument.
    std::uint64_t ts_ns = 0;  ///< Feed timestamp, nanoseconds.
    Qty bid_qty = 0;          ///< Best bid size.
    Price bid_px;             ///< Best bid price.
    Qty ask_qty = 0;          ///< Best ask size.
    Price ask_px;             ///< Best ask price.

    bool operator==(const Quote& other) const noexcept = default;
};

/// @brief GCMD/1 trade print.
struct Trade {
    Symbol symbol;                             ///< Instrument.
    std::uint64_t ts_ns = 0;                   ///< Feed timestamp, nanoseconds.
    Qty qty = 0;                               ///< Traded quantity.
    Price px;                                  ///< Traded price.
    Aggressor aggressor = Aggressor::Unknown;  ///< Initiating side.
    std::int64_t id = 0;                       ///< Trade ID.

    bool operator==(const Trade& other) const noexcept = default;
};

/// @brief GCMD/1 keep-alive.
struct Heartbeat {
    std::uint64_t ts_ns = 0;  ///< Feed timestamp, nanoseconds.

    bool operator==(const Heartbeat& other) const noexcept = default;
};

/// @brief GCMD/1 session state change (open / halt / close).
struct SessionControl {
    std::uint64_t ts_ns = 0;                  ///< Feed timestamp, nanoseconds.
    SessionState state = SessionState::Open;  ///< New session state.

    bool operator==(const SessionControl& other) const noexcept = default;
};

/// @brief GCOE/1 order sent by the engine in response to a trade.
struct NewOrder {
    std::uint64_t client_order_id = 0;           ///< Engine-assigned order ID.
    Symbol symbol;                               ///< Instrument.
    OrderStatus status = OrderStatus::Rejected;  ///< Accept/reject decision.
    std::uint64_t ts_ns = 0;                     ///< Timestamp, nanoseconds.
    std::int64_t trade_id = 0;                   ///< ID of the trade that triggered this order.
    Side side = Side::Buy;                       ///< Order side.
    Qty qty = 0;                                 ///< Order quantity.
    Price limit_px;                              ///< Limit price.

    bool operator==(const NewOrder& other) const noexcept = default;
};

/// @brief GCOE/1 execution report for an order.
struct ExecReport {
    std::uint64_t client_order_id = 0;    ///< Order this report refers to.
    std::uint64_t ts_ns = 0;              ///< Timestamp, nanoseconds.
    ExecStatus status = ExecStatus::Ack;  ///< Execution status.
    Qty filled_qty = 0;                   ///< Quantity filled so far.
    Price avg_px;                         ///< Average fill price.
    std::uint8_t reason_code = 0;         ///< Reject reason; see engine::to_reason_code().

    bool operator==(const ExecReport& other) const noexcept = default;
};

}  // namespace slipstream::codec
