#pragma once

#include <cstddef>
#include <expected>
#include "codec/messages.hpp"
#include "codec/error.hpp"
#include <span>

namespace slipstream::codec {

    [[nodiscard]] std::expected<std::size_t, EncodeError> encode(const Quote&, std::span<std::byte>) noexcept;
    [[nodiscard]] std::expected<Quote, DecodeError> decode_quote(std::span<const std::byte> body) noexcept;

    [[nodiscard]] std::expected<std::size_t, EncodeError> encode(const Trade&, std::span<std::byte>) noexcept;
    [[nodiscard]] std::expected<Trade, DecodeError> decode_trade(std::span<const std::byte> body) noexcept;

    [[nodiscard]] std::expected<std::size_t, EncodeError> encode(const Heartbeat&, std::span<std::byte>) noexcept;
    [[nodiscard]] std::expected<Heartbeat, DecodeError> decode_heartbeat(std::span<const std::byte> body) noexcept;

    [[nodiscard]] std::expected<std::size_t, EncodeError> encode(const SessionControl&, std::span<std::byte>) noexcept;
    [[nodiscard]] std::expected<SessionControl, DecodeError> decode_session_control(std::span<const std::byte> body) noexcept;

    [[nodiscard]] std::expected<std::size_t, EncodeError> encode(const NewOrder&, std::span<std::byte>) noexcept;
    [[nodiscard]] std::expected<NewOrder, DecodeError> decode_new_order(std::span<const std::byte> body) noexcept;

    [[nodiscard]] std::expected<std::size_t, EncodeError> encode(const ExecReport&, std::span<std::byte>) noexcept;
    [[nodiscard]] std::expected<ExecReport, DecodeError> decode_exec_report(std::span<const std::byte> body) noexcept;

}  // namespace slipstream::codec
