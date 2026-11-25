#pragma once

/// @file
/// @brief Fixed-width 12-byte instrument symbol.

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace slipstream::core {

/// @brief Wire symbol: exactly 12 bytes, ASCII, NUL-padded.
///
/// NOT NUL-terminated when all 12 bytes are used. Never treat the storage as a C string.
///
/// Invariant: every Symbol is canonical, i.e. once a NUL appears every following byte is NUL. Both
/// factories enforce it, so operator== (which compares all 12 bytes) agrees with view().
class Symbol {
public:
    static constexpr std::size_t kSize = 12;  ///< Width on the wire, in bytes.
    using Storage = std::array<char, kSize>;  ///< Raw byte storage.

    /// @brief The empty symbol (all NUL).
    constexpr Symbol() noexcept = default;

    /// @brief Builds a Symbol from raw wire bytes.
    /// @return nullopt if a non-NUL byte follows a NUL (e.g. "AB\0CD..."), which would otherwise
    ///         display as "AB" but compare unequal to Symbol "AB".
    static constexpr std::optional<Symbol> from_bytes(const Storage& bytes) noexcept {
        bool padding = false;
        for (const char c : bytes) {
            if (c == '\0') {
                padding = true;
            } else if (padding) {
                return std::nullopt;
            }
        }

        return Symbol{bytes};
    }

    /// @brief Builds a Symbol from a name, NUL-padding it to 12 bytes.
    /// @return nullopt if the name is empty, longer than 12 bytes, or contains a NUL.
    static constexpr std::optional<Symbol> from_string(std::string_view name) noexcept {
        if (name.empty() || name.size() > kSize || name.find('\0') != std::string_view::npos) {
            return std::nullopt;
        }

        Storage bytes{};
        for (std::size_t i = 0; i < name.size(); ++i) {
            bytes[i] = name[i];
        }

        return Symbol{bytes};
    }

    /// @brief The raw 12 bytes, including padding.
    [[nodiscard]] constexpr const Storage& bytes() const noexcept { return m_bytes; }

    /// @brief The name without trailing padding; length is 12 when the field is full.
    [[nodiscard]] constexpr std::string_view view() const noexcept {
        std::size_t len = 0;
        while (len < kSize && m_bytes[len] != '\0') {
            ++len;
        }

        return {m_bytes.data(), len};
    }

    friend constexpr bool operator==(const Symbol&, const Symbol&) noexcept = default;

private:
    constexpr explicit Symbol(const Storage& bytes) noexcept : m_bytes(bytes) {}

    Storage m_bytes{};  ///< Canonical NUL-padded bytes.
};

// The codec copies a Symbol's bytes to the wire directly.
static_assert(sizeof(Symbol) == Symbol::kSize);

}  // namespace slipstream::core
