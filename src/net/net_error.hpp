#pragma once

/// @file
/// @brief Socket error codes.

#include <cstdint>
#include <string_view>

namespace slipstream::net {

/// @brief Classification of a socket operation failure.
enum class NetErrorKind : std::uint8_t {
    WouldBlock,  ///< EAGAIN / EWOULDBLOCK: not a failure, just "nothing right now"
    PeerClosed,  ///< recv() == 0: orderly shutdown
    Reset,       ///< ECONNRESET / EPIPE
    Other,       ///< see errno_value
};

/// @brief A socket error: its kind plus the raw errno.
struct NetError {
    NetErrorKind kind = NetErrorKind::Other;  ///< Error classification.
    int errno_value = 0;                      ///< errno at the time of failure (0 if none).
};

/// @brief Human-readable description of a NetErrorKind.
[[nodiscard]] constexpr std::string_view to_string(NetErrorKind k) noexcept {
    switch (k) {
        case NetErrorKind::WouldBlock: return "would block";
        case NetErrorKind::PeerClosed: return "peer closed";
        case NetErrorKind::Reset: return "connection reset";
        case NetErrorKind::Other: return "socket error";
    }
    return "unknown";
}

}  // namespace slipstream::net
