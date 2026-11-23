#pragma once

#include <cstdint>
#include <string_view>

namespace slipstream::net {

enum class NetErrorKind : std::uint8_t {
    WouldBlock,  // EAGAIN / EWOULDBLOCK: not a failure, just "nothing right now"
    PeerClosed,  // recv() == 0: orderly shutdown
    Reset,       // ECONNRESET / EPIPE
    Other,       // see errno_value
};

struct NetError {
    NetErrorKind kind = NetErrorKind::Other;
    int errno_value = 0;
};

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
