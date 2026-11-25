#pragma once

#include <cstddef>
#include <span>
#include <cstring>
#include <bit>
#include <type_traits>

namespace slipstream::codec {

    static_assert(std::endian::native == std::endian::little,
                  "slipstream assumes little-endian");

    template<typename T>
    void store_le(std::span<std::byte> buf, std::size_t offset, T value) noexcept {
        static_assert(std::is_trivially_copyable_v<T>, "store_le copies raw bytes");
        std::memcpy(buf.data() + offset, &value, sizeof(T));
    }

    template<typename T>
    T load_le(std::span<const std::byte> buf, std::size_t offset) noexcept {
        static_assert(std::is_trivially_copyable_v<T>, "load_le copies raw bytes");
        T value;

        std::memcpy(&value, buf.data() + offset, sizeof(T));

        return value;
    }

}  // namespace slipstream::codec
