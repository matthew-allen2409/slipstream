#include "codec/codec.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "codec/byte_io.hpp"
#include "codec/wire.hpp"

namespace slipstream::codec {

namespace {

// ---- field widths -------------------------------------------------------------------------------
// store_le/load_le take their width from the C++ field type. Each field's width must therefore
// equal the gap to the next offset in wire.hpp, or changing a struct field's type would silently
// change the wire format.

static_assert(sizeof(Quote::symbol) == quote::ts_ns - quote::symbol);
static_assert(sizeof(Quote::ts_ns) == quote::bid_qty - quote::ts_ns);
static_assert(sizeof(Quote::bid_qty) == quote::bid_px - quote::bid_qty);
static_assert(sizeof(Quote::bid_px) == quote::ask_qty - quote::bid_px);
static_assert(sizeof(Quote::ask_qty) == quote::ask_px - quote::ask_qty);
static_assert(sizeof(Quote::ask_px) == quote::kSize - quote::ask_px);

static_assert(sizeof(Trade::symbol) == trade::ts_ns - trade::symbol);
static_assert(sizeof(Trade::ts_ns) == trade::qty - trade::ts_ns);
static_assert(sizeof(Trade::qty) == trade::px - trade::qty);
static_assert(sizeof(Trade::px) == trade::aggressor - trade::px);
static_assert(sizeof(Trade::aggressor) == trade::id - trade::aggressor);
static_assert(sizeof(Trade::id) == trade::kSize - trade::id);

static_assert(sizeof(Heartbeat::ts_ns) == heartbeat::kSize - heartbeat::ts_ns);

static_assert(sizeof(SessionControl::ts_ns) == session_control::state - session_control::ts_ns);
static_assert(sizeof(SessionControl::state) == session_control::kSize - session_control::state);

static_assert(sizeof(NewOrder::client_order_id) == new_order::symbol - new_order::client_order_id);
static_assert(sizeof(NewOrder::symbol) == new_order::status - new_order::symbol);
static_assert(sizeof(NewOrder::status) == new_order::ts_ns - new_order::status);
static_assert(sizeof(NewOrder::ts_ns) == new_order::trade_id - new_order::ts_ns);
static_assert(sizeof(NewOrder::trade_id) == new_order::side - new_order::trade_id);
static_assert(sizeof(NewOrder::side) == new_order::qty - new_order::side);
static_assert(sizeof(NewOrder::qty) == new_order::limit_px - new_order::qty);
static_assert(sizeof(NewOrder::limit_px) == new_order::kSize - new_order::limit_px);

static_assert(sizeof(ExecReport::client_order_id) ==
              exec_report::ts_ns - exec_report::client_order_id);
static_assert(sizeof(ExecReport::ts_ns) == exec_report::status - exec_report::ts_ns);
static_assert(sizeof(ExecReport::status) == exec_report::filled_qty - exec_report::status);
static_assert(sizeof(ExecReport::filled_qty) == exec_report::avg_px - exec_report::filled_qty);
static_assert(sizeof(ExecReport::avg_px) == exec_report::reason_code - exec_report::avg_px);
static_assert(sizeof(ExecReport::reason_code) == exec_report::kSize - exec_report::reason_code);

// The header's body_len is a uint16.
static_assert(kMaxBodySize <= UINT16_MAX);

// ---- per-message wire facts
// ----------------------------------------------------------------------- One place that ties each
// message struct to its msg_type and body size, so the shared encode prologue below cannot pair a
// type with the wrong size.

template <class M>
struct WireTraits;

template <>
struct WireTraits<Quote> {
    static constexpr MsgType type = MsgType::Quote;
    static constexpr std::size_t body_size = quote::kSize;
};
template <>
struct WireTraits<Trade> {
    static constexpr MsgType type = MsgType::Trade;
    static constexpr std::size_t body_size = trade::kSize;
};
template <>
struct WireTraits<Heartbeat> {
    static constexpr MsgType type = MsgType::Heartbeat;
    static constexpr std::size_t body_size = heartbeat::kSize;
};
template <>
struct WireTraits<SessionControl> {
    static constexpr MsgType type = MsgType::SessionControl;
    static constexpr std::size_t body_size = session_control::kSize;
};
template <>
struct WireTraits<NewOrder> {
    static constexpr MsgType type = MsgType::NewOrder;
    static constexpr std::size_t body_size = new_order::kSize;
};
template <>
struct WireTraits<ExecReport> {
    static constexpr MsgType type = MsgType::ExecReport;
    static constexpr std::size_t body_size = exec_report::kSize;
};

template <class M>
inline constexpr std::size_t kFrameSize = header::kSize + WireTraits<M>::body_size;

// Shared encode prologue: checks that the whole frame fits, writes the header, and returns the
// span the body goes into. Nothing is written when the buffer is too small.
template <class M>
std::expected<std::span<std::byte>, EncodeError> begin_frame(std::span<std::byte> out) noexcept {
    if (out.size() < kFrameSize<M>) [[unlikely]] {
        return std::unexpected{EncodeError::BufferTooSmall};
    }

    store_le(out, header::body_len, static_cast<std::uint16_t>(WireTraits<M>::body_size));
    store_le(out, header::msg_type, std::to_underlying(WireTraits<M>::type));
    store_le(out, header::version, kProtocolVersion);

    return out.subspan(header::kSize, WireTraits<M>::body_size);
}

// Symbols go through Symbol::from_bytes so a decoded Symbol is always canonical (see symbol.hpp).
std::optional<Symbol> read_symbol(std::span<const std::byte> body, std::size_t offset) noexcept {
    return Symbol::from_bytes(load_le<Symbol::Storage>(body, offset));
}

}  // namespace

// ---- Quote
// ----------------------------------------------------------------------------------------

std::expected<std::size_t, EncodeError> encode(const Quote& message,
                                               std::span<std::byte> out) noexcept {
    const auto body{begin_frame<Quote>(out)};
    if (!body) [[unlikely]] {
        return std::unexpected{body.error()};
    }

    store_le(*body, quote::symbol, message.symbol);
    store_le(*body, quote::ts_ns, message.ts_ns);
    store_le(*body, quote::bid_qty, message.bid_qty);
    store_le(*body, quote::bid_px, message.bid_px);
    store_le(*body, quote::ask_qty, message.ask_qty);
    store_le(*body, quote::ask_px, message.ask_px);

    return kFrameSize<Quote>;
}

std::expected<Quote, DecodeError> decode_quote(std::span<const std::byte> body) noexcept {
    if (body.size() != quote::kSize) [[unlikely]] {
        return std::unexpected{DecodeError::BadBodyLen};
    }

    const auto symbol{read_symbol(body, quote::symbol)};
    if (!symbol) [[unlikely]] {
        return std::unexpected{DecodeError::BadField};
    }

    return Quote{
        .symbol = *symbol,
        .ts_ns = load_le<decltype(Quote::ts_ns)>(body, quote::ts_ns),
        .bid_qty = load_le<decltype(Quote::bid_qty)>(body, quote::bid_qty),
        .bid_px = load_le<decltype(Quote::bid_px)>(body, quote::bid_px),
        .ask_qty = load_le<decltype(Quote::ask_qty)>(body, quote::ask_qty),
        .ask_px = load_le<decltype(Quote::ask_px)>(body, quote::ask_px),
    };
}

// ---- Trade
// ----------------------------------------------------------------------------------------

std::expected<std::size_t, EncodeError> encode(const Trade& message,
                                               std::span<std::byte> out) noexcept {
    const auto body{begin_frame<Trade>(out)};
    if (!body) [[unlikely]] {
        return std::unexpected{body.error()};
    }

    store_le(*body, trade::symbol, message.symbol);
    store_le(*body, trade::ts_ns, message.ts_ns);
    store_le(*body, trade::qty, message.qty);
    store_le(*body, trade::px, message.px);
    store_le(*body, trade::aggressor, message.aggressor);
    store_le(*body, trade::id, message.id);

    return kFrameSize<Trade>;
}

std::expected<Trade, DecodeError> decode_trade(std::span<const std::byte> body) noexcept {
    if (body.size() != trade::kSize) [[unlikely]] {
        return std::unexpected{DecodeError::BadBodyLen};
    }

    const auto symbol{read_symbol(body, trade::symbol)};
    const auto aggressor{load_le<decltype(Trade::aggressor)>(body, trade::aggressor)};
    if (!symbol || (aggressor != Aggressor::Buy && aggressor != Aggressor::Sell &&
                    aggressor != Aggressor::Unknown)) [[unlikely]] {
        return std::unexpected{DecodeError::BadField};
    }

    return Trade{
        .symbol = *symbol,
        .ts_ns = load_le<decltype(Trade::ts_ns)>(body, trade::ts_ns),
        .qty = load_le<decltype(Trade::qty)>(body, trade::qty),
        .px = load_le<decltype(Trade::px)>(body, trade::px),
        .aggressor = aggressor,
        .id = load_le<decltype(Trade::id)>(body, trade::id),
    };
}

// ---- Heartbeat
// ------------------------------------------------------------------------------------

std::expected<std::size_t, EncodeError> encode(const Heartbeat& message,
                                               std::span<std::byte> out) noexcept {
    const auto body{begin_frame<Heartbeat>(out)};
    if (!body) [[unlikely]] {
        return std::unexpected{body.error()};
    }

    store_le(*body, heartbeat::ts_ns, message.ts_ns);

    return kFrameSize<Heartbeat>;
}

std::expected<Heartbeat, DecodeError> decode_heartbeat(std::span<const std::byte> body) noexcept {
    if (body.size() != heartbeat::kSize) [[unlikely]] {
        return std::unexpected{DecodeError::BadBodyLen};
    }

    return Heartbeat{
        .ts_ns = load_le<decltype(Heartbeat::ts_ns)>(body, heartbeat::ts_ns),
    };
}

// ---- SessionControl
// -------------------------------------------------------------------------------

std::expected<std::size_t, EncodeError> encode(const SessionControl& message,
                                               std::span<std::byte> out) noexcept {
    const auto body{begin_frame<SessionControl>(out)};
    if (!body) [[unlikely]] {
        return std::unexpected{body.error()};
    }

    store_le(*body, session_control::ts_ns, message.ts_ns);
    store_le(*body, session_control::state, message.state);

    return kFrameSize<SessionControl>;
}

std::expected<SessionControl, DecodeError> decode_session_control(
    std::span<const std::byte> body) noexcept {
    if (body.size() != session_control::kSize) [[unlikely]] {
        return std::unexpected{DecodeError::BadBodyLen};
    }

    // Loading straight into the enum is well-defined: it has a fixed underlying type, so every
    // byte value is representable and can be range-checked afterwards.
    const auto state{load_le<decltype(SessionControl::state)>(body, session_control::state)};
    if (state != SessionState::Open && state != SessionState::Halt && state != SessionState::Close)
        [[unlikely]] {
        return std::unexpected{DecodeError::BadField};
    }

    return SessionControl{
        .ts_ns = load_le<decltype(SessionControl::ts_ns)>(body, session_control::ts_ns),
        .state = state,
    };
}

// ---- NewOrder
// -------------------------------------------------------------------------------------

std::expected<std::size_t, EncodeError> encode(const NewOrder& message,
                                               std::span<std::byte> out) noexcept {
    const auto body{begin_frame<NewOrder>(out)};
    if (!body) [[unlikely]] {
        return std::unexpected{body.error()};
    }

    store_le(*body, new_order::client_order_id, message.client_order_id);
    store_le(*body, new_order::symbol, message.symbol);
    store_le(*body, new_order::status, message.status);
    store_le(*body, new_order::ts_ns, message.ts_ns);
    store_le(*body, new_order::trade_id, message.trade_id);
    store_le(*body, new_order::side, message.side);
    store_le(*body, new_order::qty, message.qty);
    store_le(*body, new_order::limit_px, message.limit_px);

    return kFrameSize<NewOrder>;
}

std::expected<NewOrder, DecodeError> decode_new_order(std::span<const std::byte> body) noexcept {
    if (body.size() != new_order::kSize) [[unlikely]] {
        return std::unexpected{DecodeError::BadBodyLen};
    }

    const auto symbol{read_symbol(body, new_order::symbol)};
    const auto status{load_le<decltype(NewOrder::status)>(body, new_order::status)};
    const auto side{load_le<decltype(NewOrder::side)>(body, new_order::side)};
    if (!symbol || (status != OrderStatus::Accepted && status != OrderStatus::Rejected) ||
        (side != Side::Buy && side != Side::Sell)) [[unlikely]] {
        return std::unexpected{DecodeError::BadField};
    }

    return NewOrder{
        .client_order_id =
            load_le<decltype(NewOrder::client_order_id)>(body, new_order::client_order_id),
        .symbol = *symbol,
        .status = status,
        .ts_ns = load_le<decltype(NewOrder::ts_ns)>(body, new_order::ts_ns),
        .trade_id = load_le<decltype(NewOrder::trade_id)>(body, new_order::trade_id),
        .side = side,
        .qty = load_le<decltype(NewOrder::qty)>(body, new_order::qty),
        .limit_px = load_le<decltype(NewOrder::limit_px)>(body, new_order::limit_px),
    };
}

// ---- ExecReport
// -----------------------------------------------------------------------------------

std::expected<std::size_t, EncodeError> encode(const ExecReport& message,
                                               std::span<std::byte> out) noexcept {
    const auto body{begin_frame<ExecReport>(out)};
    if (!body) [[unlikely]] {
        return std::unexpected{body.error()};
    }

    store_le(*body, exec_report::client_order_id, message.client_order_id);
    store_le(*body, exec_report::ts_ns, message.ts_ns);
    store_le(*body, exec_report::status, message.status);
    store_le(*body, exec_report::filled_qty, message.filled_qty);
    store_le(*body, exec_report::avg_px, message.avg_px);
    store_le(*body, exec_report::reason_code, message.reason_code);

    return kFrameSize<ExecReport>;
}

std::expected<ExecReport, DecodeError> decode_exec_report(
    std::span<const std::byte> body) noexcept {
    if (body.size() != exec_report::kSize) [[unlikely]] {
        return std::unexpected{DecodeError::BadBodyLen};
    }

    const auto status{load_le<decltype(ExecReport::status)>(body, exec_report::status)};
    if (status != ExecStatus::Ack && status != ExecStatus::Fill && status != ExecStatus::Partial &&
        status != ExecStatus::Reject) [[unlikely]] {
        return std::unexpected{DecodeError::BadField};
    }

    // reason_code is a plain uint8 on the wire (not an enum), so every value is accepted.
    return ExecReport{
        .client_order_id =
            load_le<decltype(ExecReport::client_order_id)>(body, exec_report::client_order_id),
        .ts_ns = load_le<decltype(ExecReport::ts_ns)>(body, exec_report::ts_ns),
        .status = status,
        .filled_qty = load_le<decltype(ExecReport::filled_qty)>(body, exec_report::filled_qty),
        .avg_px = load_le<decltype(ExecReport::avg_px)>(body, exec_report::avg_px),
        .reason_code = load_le<decltype(ExecReport::reason_code)>(body, exec_report::reason_code),
    };
}

}  // namespace slipstream::codec
