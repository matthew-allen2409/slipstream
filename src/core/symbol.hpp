#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace slipstream::core {

// Wire symbol: exactly 12 bytes, ASCII, NUL-padded, and NOT NUL-terminated when all 12 bytes are
// used. Never treat the storage as a C string.
class Symbol {
public:
    static constexpr std::size_t kSize = 12;
    using Storage = std::array<char, kSize>;

    constexpr Symbol() noexcept = default;

    static constexpr Symbol from_bytes(const Storage& bytes) noexcept { return Symbol{bytes}; }

    // Returns nullopt if the name is empty or longer than 12 bytes.
    static constexpr std::optional<Symbol> from_string(std::string_view name) noexcept {
        if (name.empty() || name.size() > kSize) {
            return std::nullopt;
        }
        Storage bytes{};
        for (std::size_t i = 0; i < name.size(); ++i) {
            bytes[i] = name[i];
        }
        return Symbol{bytes};
    }

    [[nodiscard]] constexpr const Storage& bytes() const noexcept { return bytes_; }

    // The name without trailing padding; length is 12 when the field is full.
    [[nodiscard]] constexpr std::string_view view() const noexcept {
        std::size_t len = 0;
        while (len < kSize && bytes_[len] != '\0') {
            ++len;
        }
        return {bytes_.data(), len};
    }

    friend constexpr bool operator==(const Symbol&, const Symbol&) noexcept = default;

private:
    constexpr explicit Symbol(const Storage& bytes) noexcept : bytes_(bytes) {}

    Storage bytes_{};
};

}  // namespace slipstream::core
