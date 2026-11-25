#include <cstddef>
#include <cstdint>
#include <utility>

#include "codec/codec.hpp"
#include "codec/byte_io.hpp"
#include "codec/wire.hpp"

namespace slipstream::codec {

    namespace {
        // Writes the frame header and returns the part of `out` where the body goes.
        // The caller has already checked that `out` is large enough for the whole frame.
        std::span<std::byte> write_header(std::span<std::byte> out, MsgType type, std::size_t body_size) noexcept {
            store_le(out, header::body_len, static_cast<std::uint16_t>(body_size));
            store_le(out, header::msg_type, std::to_underlying(type));
            store_le(out, header::version, kProtocolVersion);

            return out.subspan(header::kSize);
        }
    }

    // ---- Quote -----------------------------------------------------------------------------------

    std::expected<std::size_t, EncodeError> encode(const Quote& message, std::span<std::byte> out) noexcept {
        constexpr std::size_t frame_size{header::kSize + quote::kSize};

        if (out.size() < frame_size) {
            return std::unexpected{EncodeError::BufferTooSmall};
        }

        auto body{write_header(out, MsgType::Quote, quote::kSize)};
        store_le(body, quote::symbol, message.symbol);
        store_le(body, quote::ts_ns, message.ts_ns);
        store_le(body, quote::bid_qty, message.bid_qty);
        store_le(body, quote::bid_px, message.bid_px);
        store_le(body, quote::ask_qty, message.ask_qty);
        store_le(body, quote::ask_px, message.ask_px);

        return frame_size;
    }

    std::expected<Quote, DecodeError> decode_quote(std::span<const std::byte> body) noexcept {
        if (body.size() != quote::kSize) {
            return std::unexpected{DecodeError::BadBodyLen};
        }

        return Quote{
            .symbol = load_le<decltype(Quote::symbol)>(body, quote::symbol),
            .ts_ns = load_le<decltype(Quote::ts_ns)>(body, quote::ts_ns),
            .bid_qty = load_le<decltype(Quote::bid_qty)>(body, quote::bid_qty),
            .bid_px = load_le<decltype(Quote::bid_px)>(body, quote::bid_px),
            .ask_qty = load_le<decltype(Quote::ask_qty)>(body, quote::ask_qty),
            .ask_px = load_le<decltype(Quote::ask_px)>(body, quote::ask_px),
        };
    }

    // ---- Trade -----------------------------------------------------------------------------------

    std::expected<std::size_t, EncodeError> encode(const Trade& message, std::span<std::byte> out) noexcept {
        constexpr std::size_t frame_size{header::kSize + trade::kSize};

        if (out.size() < frame_size) {
            return std::unexpected{EncodeError::BufferTooSmall};
        }

        auto body{write_header(out, MsgType::Trade, trade::kSize)};
        store_le(body, trade::symbol, message.symbol);
        store_le(body, trade::ts_ns, message.ts_ns);
        store_le(body, trade::qty, message.qty);
        store_le(body, trade::px, message.px);
        store_le(body, trade::aggressor, message.aggressor);
        store_le(body, trade::id, message.id);

        return frame_size;
    }

    std::expected<Trade, DecodeError> decode_trade(std::span<const std::byte> body) noexcept {
        if (body.size() != trade::kSize) {
            return std::unexpected{DecodeError::BadBodyLen};
        }

        const auto aggressor{load_le<decltype(Trade::aggressor)>(body, trade::aggressor)};
        if (aggressor != Aggressor::Buy && aggressor != Aggressor::Sell && aggressor != Aggressor::Unknown) {
            return std::unexpected{DecodeError::BadField};
        }

        return Trade{
            .symbol = load_le<decltype(Trade::symbol)>(body, trade::symbol),
            .ts_ns = load_le<decltype(Trade::ts_ns)>(body, trade::ts_ns),
            .qty = load_le<decltype(Trade::qty)>(body, trade::qty),
            .px = load_le<decltype(Trade::px)>(body, trade::px),
            .aggressor = aggressor,
            .id = load_le<decltype(Trade::id)>(body, trade::id),
        };
    }

    // ---- Heartbeat -------------------------------------------------------------------------------

    std::expected<std::size_t, EncodeError> encode(const Heartbeat& message, std::span<std::byte> out) noexcept {
        constexpr std::size_t frame_size{header::kSize + heartbeat::kSize};

        if (out.size() < frame_size) {
            return std::unexpected{EncodeError::BufferTooSmall};
        }

        auto body{write_header(out, MsgType::Heartbeat, heartbeat::kSize)};
        store_le(body, heartbeat::ts_ns, message.ts_ns);

        return frame_size;
    }

    std::expected<Heartbeat, DecodeError> decode_heartbeat(std::span<const std::byte> body) noexcept {
        if (body.size() != heartbeat::kSize) {
            return std::unexpected{DecodeError::BadBodyLen};
        }

        return Heartbeat{
            .ts_ns = load_le<decltype(Heartbeat::ts_ns)>(body, heartbeat::ts_ns),
        };
    }

    // ---- SessionControl --------------------------------------------------------------------------

    std::expected<std::size_t, EncodeError> encode(const SessionControl& message, std::span<std::byte> out) noexcept {
        constexpr std::size_t frame_size{header::kSize + session_control::kSize};

        if (out.size() < frame_size) {
            return std::unexpected{EncodeError::BufferTooSmall};
        }

        auto body{write_header(out, MsgType::SessionControl, session_control::kSize)};
        store_le(body, session_control::ts_ns, message.ts_ns);
        store_le(body, session_control::state, message.state);

        return frame_size;
    }

    std::expected<SessionControl, DecodeError> decode_session_control(std::span<const std::byte> body) noexcept {
        if (body.size() != session_control::kSize) {
            return std::unexpected{DecodeError::BadBodyLen};
        }

        const auto state{load_le<decltype(SessionControl::state)>(body, session_control::state)};
        if (state != SessionState::Open && state != SessionState::Halt && state != SessionState::Close) {
            return std::unexpected{DecodeError::BadField};
        }

        return SessionControl{
            .ts_ns = load_le<decltype(SessionControl::ts_ns)>(body, session_control::ts_ns),
            .state = state,
        };
    }

    // ---- NewOrder --------------------------------------------------------------------------------

    std::expected<std::size_t, EncodeError> encode(const NewOrder& message, std::span<std::byte> out) noexcept {
        constexpr std::size_t frame_size{header::kSize + new_order::kSize};

        if (out.size() < frame_size) {
            return std::unexpected{EncodeError::BufferTooSmall};
        }

        auto body{write_header(out, MsgType::NewOrder, new_order::kSize)};
        store_le(body, new_order::client_order_id, message.client_order_id);
        store_le(body, new_order::symbol, message.symbol);
        store_le(body, new_order::status, message.status);
        store_le(body, new_order::ts_ns, message.ts_ns);
        store_le(body, new_order::trade_id, message.trade_id);
        store_le(body, new_order::side, message.side);
        store_le(body, new_order::qty, message.qty);
        store_le(body, new_order::limit_px, message.limit_px);

        return frame_size;
    }

    std::expected<NewOrder, DecodeError> decode_new_order(std::span<const std::byte> body) noexcept {
        if (body.size() != new_order::kSize) {
            return std::unexpected{DecodeError::BadBodyLen};
        }

        const auto status{load_le<decltype(NewOrder::status)>(body, new_order::status)};
        if (status != OrderStatus::Accepted && status != OrderStatus::Rejected) {
            return std::unexpected{DecodeError::BadField};
        }

        const auto side{load_le<decltype(NewOrder::side)>(body, new_order::side)};
        if (side != Side::Buy && side != Side::Sell) {
            return std::unexpected{DecodeError::BadField};
        }

        return NewOrder{
            .client_order_id = load_le<decltype(NewOrder::client_order_id)>(body, new_order::client_order_id),
            .symbol = load_le<decltype(NewOrder::symbol)>(body, new_order::symbol),
            .status = status,
            .ts_ns = load_le<decltype(NewOrder::ts_ns)>(body, new_order::ts_ns),
            .trade_id = load_le<decltype(NewOrder::trade_id)>(body, new_order::trade_id),
            .side = side,
            .qty = load_le<decltype(NewOrder::qty)>(body, new_order::qty),
            .limit_px = load_le<decltype(NewOrder::limit_px)>(body, new_order::limit_px),
        };
    }

    // ---- ExecReport ------------------------------------------------------------------------------

    std::expected<std::size_t, EncodeError> encode(const ExecReport& message, std::span<std::byte> out) noexcept {
        constexpr std::size_t frame_size{header::kSize + exec_report::kSize};

        if (out.size() < frame_size) {
            return std::unexpected{EncodeError::BufferTooSmall};
        }

        auto body{write_header(out, MsgType::ExecReport, exec_report::kSize)};
        store_le(body, exec_report::client_order_id, message.client_order_id);
        store_le(body, exec_report::ts_ns, message.ts_ns);
        store_le(body, exec_report::status, message.status);
        store_le(body, exec_report::filled_qty, message.filled_qty);
        store_le(body, exec_report::avg_px, message.avg_px);
        store_le(body, exec_report::reason_code, message.reason_code);

        return frame_size;
    }

    std::expected<ExecReport, DecodeError> decode_exec_report(std::span<const std::byte> body) noexcept {
        if (body.size() != exec_report::kSize) {
            return std::unexpected{DecodeError::BadBodyLen};
        }

        const auto status{load_le<decltype(ExecReport::status)>(body, exec_report::status)};
        if (status != ExecStatus::Ack && status != ExecStatus::Fill &&
            status != ExecStatus::Partial && status != ExecStatus::Reject) {
            return std::unexpected{DecodeError::BadField};
        }

        return ExecReport{
            .client_order_id = load_le<decltype(ExecReport::client_order_id)>(body, exec_report::client_order_id),
            .ts_ns = load_le<decltype(ExecReport::ts_ns)>(body, exec_report::ts_ns),
            .status = status,
            .filled_qty = load_le<decltype(ExecReport::filled_qty)>(body, exec_report::filled_qty),
            .avg_px = load_le<decltype(ExecReport::avg_px)>(body, exec_report::avg_px),
            .reason_code = load_le<decltype(ExecReport::reason_code)>(body, exec_report::reason_code),
        };
    }

}  // namespace slipstream::codec
