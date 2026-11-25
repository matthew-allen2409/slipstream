#pragma once

/// @file
/// @brief Codec error codes.
///
/// Error enums are trivially copyable and never allocate, so std::expected<T, E> is safe on the
/// hot path.

#include <cstdint>
#include <string_view>

namespace slipstream::codec {

/// @brief Why a frame or body could not be decoded.
///
/// "Need more bytes" is deliberately NOT an error: the parser reports it as an empty optional.
enum class DecodeError : std::uint8_t {
    BadVersion,   ///< header.version != kProtocolVersion
    UnknownType,  ///< msg_type not in MsgType
    BadBodyLen,   ///< body_len != expected_body_size(msg_type)
    BadField,     ///< e.g. aggressor not in {'B','S','?'}, session state > 2
};

/// @brief Why a message could not be encoded.
enum class EncodeError : std::uint8_t {
    BufferTooSmall,  ///< Output buffer shorter than the full frame.
};

/// @brief Human-readable description of a DecodeError.
[[nodiscard]] constexpr std::string_view to_string(DecodeError e) noexcept {
    switch (e) {
        case DecodeError::BadVersion: return "bad version";
        case DecodeError::UnknownType: return "unknown message type";
        case DecodeError::BadBodyLen: return "body length does not match message type";
        case DecodeError::BadField: return "invalid field value";
    }
    return "unknown decode error";
}

/// @brief Human-readable description of an EncodeError.
[[nodiscard]] constexpr std::string_view to_string(EncodeError e) noexcept {
    switch (e) {
        case EncodeError::BufferTooSmall: return "output buffer too small";
    }
    return "unknown encode error";
}

}  // namespace slipstream::codec
