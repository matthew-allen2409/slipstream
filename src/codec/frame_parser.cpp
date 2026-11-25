#include "codec/frame_parser.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "codec/byte_io.hpp"
#include "codec/error.hpp"
#include "codec/wire.hpp"

namespace slipstream::codec {

namespace {

struct Header {
    std::uint16_t body_len;
    MsgType msg_type;
    std::uint8_t version;
};

Header read_header(std::span<const std::byte, header::kSize> bytes) noexcept {
    return Header{
        .body_len = load_le<decltype(Header::body_len)>(bytes, header::body_len),
        .msg_type = load_le<decltype(Header::msg_type)>(bytes, header::msg_type),
        .version = load_le<decltype(Header::version)>(bytes, header::version),
    };
}

}  // namespace

void FrameParser::compact() noexcept {
    const std::size_t count{buffered()};
    std::memmove(m_buf.data(), m_buf.data() + m_begin, count);
    m_begin = 0;
    m_end = count;
}

std::size_t FrameParser::buffered() const noexcept {
    return m_end - m_begin;
}

std::size_t FrameParser::feed(std::span<const std::byte> data) noexcept {
    const std::size_t remaining_capacity{kCapacity - buffered()};
    const std::size_t accepted{std::min(remaining_capacity, data.size())};

    if (accepted == 0) [[unlikely]] {
        return 0;
    }

    if ((m_end + accepted) > kCapacity) {
        compact();
    }

    std::memcpy(m_buf.data() + m_end, data.data(), accepted);
    m_end += accepted;

    return accepted;
}

std::expected<std::optional<Frame>, DecodeError> FrameParser::next() noexcept {
    if (buffered() < header::kSize) {
        return std::nullopt;
    }

    const auto header_view{std::span{m_buf}.subspan(m_begin).first<header::kSize>()};
    const Header header{read_header(header_view)};

    // The error returns below leave m_begin where it is: the bad header stays at the front of the
    // buffer, so every later call fails the same way. That is what makes errors sticky.
    if (header.version != kProtocolVersion) [[unlikely]] {
        return std::unexpected{DecodeError::BadVersion};
    }

    const std::size_t expected_size{expected_body_size(header.msg_type)};
    if (expected_size == 0) [[unlikely]] {
        return std::unexpected{DecodeError::UnknownType};
    }

    if (header.body_len != expected_size) [[unlikely]] {
        return std::unexpected{DecodeError::BadBodyLen};
    }

    const std::size_t frame_size{header::kSize + header.body_len};
    if (buffered() < frame_size) {
        return std::nullopt;
    }

    const auto body{std::span{m_buf}.subspan(m_begin + header::kSize).first(header.body_len)};

    m_begin += frame_size;
    // Fully drained: start again at the front, so a stream of whole frames never needs compact().
    // Safe here because `body` stays valid: the bytes are not touched until the next feed().
    if (m_begin == m_end) {
        m_begin = 0;
        m_end = 0;
    }

    return Frame{
        .type = header.msg_type,
        .body = body,
    };
}

}  // namespace slipstream::codec
