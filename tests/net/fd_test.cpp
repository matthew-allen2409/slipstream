#include "net/fd.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <utility>

namespace {

using slipstream::net::Fd;

bool is_open(int fd) { return ::fcntl(fd, F_GETFD) != -1 || errno != EBADF; }

std::array<int, 2> make_pipe() {
    std::array<int, 2> fds{-1, -1};
    EXPECT_EQ(::pipe2(fds.data(), O_CLOEXEC), 0);
    return fds;
}

TEST(Fd, DefaultIsInvalid) {
    const Fd fd;
    EXPECT_FALSE(fd.valid());
    EXPECT_FALSE(static_cast<bool>(fd));
}

TEST(Fd, ClosesOnDestruction) {
    const auto fds = make_pipe();
    ::close(fds[1]);
    {
        const Fd fd{fds[0]};
        EXPECT_TRUE(is_open(fds[0]));
    }
    EXPECT_FALSE(is_open(fds[0]));
}

TEST(Fd, MoveTransfersOwnership) {
    const auto fds = make_pipe();
    ::close(fds[1]);
    Fd a{fds[0]};
    Fd b{std::move(a)};
    EXPECT_FALSE(a.valid());  // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(b.get(), fds[0]);
    EXPECT_TRUE(is_open(fds[0]));
}

TEST(Fd, MoveAssignClosesPrevious) {
    const auto p1 = make_pipe();
    const auto p2 = make_pipe();
    ::close(p1[1]);
    ::close(p2[1]);
    Fd a{p1[0]};
    Fd b{p2[0]};
    a = std::move(b);
    EXPECT_FALSE(is_open(p1[0]));
    EXPECT_EQ(a.get(), p2[0]);
}

TEST(Fd, ReleaseDoesNotClose) {
    const auto fds = make_pipe();
    ::close(fds[1]);
    int raw = -1;
    {
        Fd fd{fds[0]};
        raw = fd.release();
    }
    EXPECT_TRUE(is_open(raw));
    ::close(raw);
}

}  // namespace
