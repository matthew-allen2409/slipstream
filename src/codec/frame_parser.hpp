#pragma once

/// @file
/// @brief Reassembles length-prefixed frames from a byte stream.

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>

#include "codec/error.hpp"
#include "codec/wire.hpp"

namespace slipstream::codec {

/// @brief One complete frame whose header has been validated, with the header stripped.
struct Frame {
    MsgType type;  ///< Message type from the header.
    /// @brief The message body: a view into the parser's buffer, not a copy.
    ///
    /// Valid until the next call to FrameParser::feed() or FrameParser::next(); decode or copy it
    /// before then.
    std::span<const std::byte> body;
};

/// @brief Reassembles frames from a TCP byte stream. One instance per connection.
///
/// Usage: feed() the bytes from each recv(), then call next() until it returns std::nullopt.
/// Never allocates; the buffer is a fixed-size member.
class FrameParser {
public:
    /// @brief Size of the internal buffer in bytes.
    static constexpr std::size_t kCapacity{4096};
    static_assert(kCapacity >= header::kSize + kMaxBodySize);

    /// @brief Bytes accepted by feed() and not yet returned as part of a frame.
    [[nodiscard]] std::size_t buffered() const noexcept;

    /// @brief Copies as much of @p data as fits into the buffer.
    ///
    /// Accepts fewer than data.size() only when the buffer is full; the caller then drains frames
    /// with next() and feeds the remainder.
    /// @return How many bytes were accepted.
    [[nodiscard]] std::size_t feed(std::span<const std::byte> data) noexcept;

    /// @brief Extracts the next complete frame, if one is buffered.
    ///
    /// A TCP stream cannot be resynchronised after a bad header, so once next() has returned an
    /// error it returns that same error forever. The caller should close the connection.
    ///
    /// @return
    ///   - a Frame: the next complete frame
    ///   - std::nullopt: not enough bytes yet for a whole frame (not an error)
    ///   - a DecodeError: BadVersion, UnknownType or BadBodyLen, reported as soon as the header
    ///     arrives, without waiting for the body.
    [[nodiscard]] std::expected<std::optional<Frame>, DecodeError> next() noexcept;

private:
    /// @brief Moves the unconsumed bytes [m_begin, m_end) to the front of the buffer.
    void compact() noexcept;

    std::array<std::byte, kCapacity> m_buf{};  ///< Received bytes.
    std::size_t m_begin = 0;                   ///< First unconsumed byte.
    std::size_t m_end = 0;                     ///< One past the last buffered byte.
};

}  // namespace slipstream::codec
