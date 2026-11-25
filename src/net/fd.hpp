#pragma once

/// @file
/// @brief RAII file descriptor.

namespace slipstream::net {

/// @brief Owning, move-only file descriptor. Closes on destruction.
class Fd {
public:
    /// @brief An empty (invalid) descriptor.
    Fd() noexcept = default;
    /// @brief Takes ownership of @p fd.
    explicit Fd(int fd) noexcept : fd_(fd) {}

    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;

    /// @brief Takes ownership from @p other, leaving it empty.
    Fd(Fd&& other) noexcept : fd_(other.release()) {}
    /// @brief Closes the current descriptor and takes ownership from @p other.
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    ~Fd() { reset(); }

    /// @brief The raw descriptor, or -1 if empty. Ownership is kept.
    [[nodiscard]] int get() const noexcept { return fd_; }
    /// @brief True if a descriptor is held.
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    /// @brief Same as valid().
    explicit operator bool() const noexcept { return valid(); }

    /// @brief Gives up ownership without closing.
    /// @return The descriptor previously held (or -1).
    [[nodiscard]] int release() noexcept {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    /// @brief Closes the current descriptor (if any) and takes ownership of @p fd.
    void reset(int fd = -1) noexcept;

private:
    int fd_ = -1;
};

}  // namespace slipstream::net
