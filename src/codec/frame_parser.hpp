#pragma once

#include <array>
#include <cstddef>

#include "codec/wire.hpp"

namespace slipstream::codec {

// Reassembles frames from a TCP byte stream. One instance per connection.
//
// TODO(M1): feed(std::span<const std::byte>) and
//   next() -> std::expected<std::optional<Frame>, DecodeError>
// with split-frame tests at every byte boundary.
class FrameParser {
public:
    static constexpr std::size_t kCapacity = 4096;
    static_assert(kCapacity >= header::kSize + kMaxBodySize);

    [[nodiscard]] std::size_t buffered() const noexcept;

private:
    [[maybe_unused]] std::array<std::byte, kCapacity> buf_{};  // TODO(M1): drop attribute
    std::size_t begin_ = 0;
    std::size_t end_ = 0;
};

}  // namespace slipstream::codec
