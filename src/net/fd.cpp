#include "net/fd.hpp"

#include <unistd.h>

namespace slipstream::net {

void Fd::reset(int fd) noexcept {
    if (fd_ >= 0 && fd_ != fd) {
        // On Linux the descriptor is released even when close() reports EINTR, so never retry.
        ::close(fd_);
    }
    fd_ = fd;
}

}  // namespace slipstream::net
