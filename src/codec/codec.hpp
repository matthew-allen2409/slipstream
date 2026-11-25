#pragma once

// Encoders and decoders for every GCMD/1 and GCOE/1 message.
//
// encode() writes header + body into a caller-supplied buffer and returns the bytes written, so
// nothing allocates. decode_*() take the BODY only: the frame parser has already read the header
// and used msg_type to choose the decoder.

#include <cstddef>
#include <expected>
#include <span>
#include <type_traits>

#include "codec/error.hpp"
#include "codec/messages.hpp"

namespace slipstream::codec {

[[nodiscard]] std::expected<std::size_t, EncodeError> encode(const Quote&,
                                                             std::span<std::byte>) noexcept;
[[nodiscard]] std::expected<std::size_t, EncodeError> encode(const Trade&,
                                                             std::span<std::byte>) noexcept;
[[nodiscard]] std::expected<std::size_t, EncodeError> encode(const Heartbeat&,
                                                             std::span<std::byte>) noexcept;
[[nodiscard]] std::expected<std::size_t, EncodeError> encode(const SessionControl&,
                                                             std::span<std::byte>) noexcept;
[[nodiscard]] std::expected<std::size_t, EncodeError> encode(const NewOrder&,
                                                             std::span<std::byte>) noexcept;
[[nodiscard]] std::expected<std::size_t, EncodeError> encode(const ExecReport&,
                                                             std::span<std::byte>) noexcept;

[[nodiscard]] std::expected<Quote, DecodeError> decode_quote(
    std::span<const std::byte> body) noexcept;
[[nodiscard]] std::expected<Trade, DecodeError> decode_trade(
    std::span<const std::byte> body) noexcept;
[[nodiscard]] std::expected<Heartbeat, DecodeError> decode_heartbeat(
    std::span<const std::byte> body) noexcept;
[[nodiscard]] std::expected<SessionControl, DecodeError> decode_session_control(
    std::span<const std::byte> body) noexcept;
[[nodiscard]] std::expected<NewOrder, DecodeError> decode_new_order(
    std::span<const std::byte> body) noexcept;
[[nodiscard]] std::expected<ExecReport, DecodeError> decode_exec_report(
    std::span<const std::byte> body) noexcept;

// decode<Quote>(body) == decode_quote(body). C++ cannot overload on return type alone, so this
// gives generic code (the frame parser's dispatch, tests) one name to call by message type.
template <class M>
[[nodiscard]] std::expected<M, DecodeError> decode(std::span<const std::byte> body) noexcept {
    if constexpr (std::is_same_v<M, Quote>) {
        return decode_quote(body);
    } else if constexpr (std::is_same_v<M, Trade>) {
        return decode_trade(body);
    } else if constexpr (std::is_same_v<M, Heartbeat>) {
        return decode_heartbeat(body);
    } else if constexpr (std::is_same_v<M, SessionControl>) {
        return decode_session_control(body);
    } else if constexpr (std::is_same_v<M, NewOrder>) {
        return decode_new_order(body);
    } else if constexpr (std::is_same_v<M, ExecReport>) {
        return decode_exec_report(body);
    } else {
        static_assert(false, "decode<M>: M is not a wire message type");
    }
}

}  // namespace slipstream::codec
