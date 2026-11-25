#pragma once

// Little-endian load/store of fixed-width values at a byte offset.
//
// std::memcpy (rather than reinterpret_cast or packed structs) avoids strict-aliasing UB and
// unaligned access; with a constant size the compiler turns it into a single mov.

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>

namespace slipstream::codec {

static_assert(std::endian::native == std::endian::little, "slipstream assumes little-endian");

// Precondition: offset + sizeof(T) <= buf.size(). Checked by assert in debug builds only, so the
// hot path pays nothing in release; callers validate frame/body sizes up front.
template <typename T>
void store_le(std::span<std::byte> buf, std::size_t offset, T value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "store_le copies raw bytes");
    assert(offset + sizeof(T) <= buf.size());
    std::memcpy(buf.data() + offset, &value, sizeof(T));
}

template <typename T>
T load_le(std::span<const std::byte> buf, std::size_t offset) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "load_le copies raw bytes");
    assert(offset + sizeof(T) <= buf.size());
    T value;
    std::memcpy(&value, buf.data() + offset, sizeof(T));
    return value;
}

}  // namespace slipstream::codec
