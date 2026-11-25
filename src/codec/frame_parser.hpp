#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>

#include "codec/error.hpp"
#include "codec/wire.hpp"

namespace slipstream::codec {

// One complete frame whose header has been validated, with the header stripped.
struct Frame {
    MsgType type;
    // A view into the parser's buffer, not a copy. Valid until the next call to
    // FrameParser::feed() or FrameParser::next(); decode or copy it before then.
    std::span<const std::byte> body;
};

// Reassembles frames from a TCP byte stream. One instance per connection.
//
// Usage: feed() the bytes from each recv(), then call next() until it returns std::nullopt.
// Never allocates; the buffer is a fixed-size member.
class FrameParser {
public:
    static constexpr std::size_t kCapacity{4096};
    static_assert(kCapacity >= header::kSize + kMaxBodySize);

    // Bytes accepted by feed() and not yet returned as part of a frame.
    [[nodiscard]] std::size_t buffered() const noexcept;

    // Copies as much of `data` as fits and returns how many bytes were accepted. Accepts fewer
    // than data.size() only when the buffer is full; the caller then drains frames with next()
    // and feeds the remainder.
    [[nodiscard]] std::size_t feed(std::span<const std::byte> data) noexcept;

    // Returns:
    //   a Frame        the next complete frame
    //   std::nullopt   not enough bytes yet for a whole frame (not an error)
    //   a DecodeError  BadVersion, UnknownType or BadBodyLen, reported as soon as the header
    //                  arrives, without waiting for the body.
    // A TCP stream cannot be resynchronised after a bad header, so once next() has returned an
    // error it returns that same error forever. The caller should close the connection.
    [[nodiscard]] std::expected<std::optional<Frame>, DecodeError> next() noexcept;

private:
    // Moves the unconsumed bytes [m_begin, m_end) to the front of the buffer.
    void compact() noexcept;

    std::array<std::byte, kCapacity> m_buf{};
    std::size_t m_begin = 0;  // first unconsumed byte
    std::size_t m_end = 0;    // one past the last buffered byte
};

}  // namespace slipstream::codec
