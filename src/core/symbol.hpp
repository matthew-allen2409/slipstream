#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace slipstream::core {

class Symbol {
public:
    static constexpr std::size_t kSize = 12;
    using Storage = std::array<char, kSize>;

    constexpr Symbol() noexcept = default;

    static constexpr Symbol from_bytes(const Storage& bytes) noexcept { return Symbol{bytes}; }

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

    [[nodiscard]] constexpr const Storage& bytes() const noexcept { return m_bytes; }

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

    Storage m_bytes{};
};

// The codec copies Symbol to and from the wire as raw bytes.
static_assert(sizeof(Symbol) == Symbol::kSize);

}  // namespace slipstream::core
