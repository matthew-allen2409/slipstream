#include "support/alloc_guard.hpp"

#include <cstdlib>
#include <new>

namespace {

constinit thread_local std::size_t t_alloc_count = 0;
constinit thread_local std::size_t t_alloc_bytes = 0;

void* counted_alloc(std::size_t size) noexcept {
    ++t_alloc_count;
    t_alloc_bytes += size;
    return std::malloc(size == 0 ? 1 : size);
}

void* counted_aligned_alloc(std::size_t size, std::align_val_t align) noexcept {
    ++t_alloc_count;
    t_alloc_bytes += size;
    const auto a = static_cast<std::size_t>(align);
    const std::size_t rounded = ((size == 0 ? 1 : size) + a - 1) / a * a;  // aligned_alloc rule
    return std::aligned_alloc(a, rounded);
}

void* or_throw(void* p) {
    if (p == nullptr) {
        throw std::bad_alloc{};
    }
    return p;
}

}  // namespace

namespace slipstream::test {

AllocGuard::AllocGuard() noexcept : start_count_(t_alloc_count), start_bytes_(t_alloc_bytes) {}

std::size_t AllocGuard::allocations() const noexcept { return t_alloc_count - start_count_; }

std::size_t AllocGuard::bytes() const noexcept { return t_alloc_bytes - start_bytes_; }

}  // namespace slipstream::test

// ---- replaceable global allocation functions ------------------------------------------------
// NOLINTBEGIN(cppcoreguidelines-no-malloc)

void* operator new(std::size_t size) { return or_throw(counted_alloc(size)); }
void* operator new[](std::size_t size) { return or_throw(counted_alloc(size)); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return counted_alloc(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return counted_alloc(size);
}
void* operator new(std::size_t size, std::align_val_t al) {
    return or_throw(counted_aligned_alloc(size, al));
}
void* operator new[](std::size_t size, std::align_val_t al) {
    return or_throw(counted_aligned_alloc(size, al));
}
void* operator new(std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept {
    return counted_aligned_alloc(size, al);
}
void* operator new[](std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept {
    return counted_aligned_alloc(size, al);
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    std::free(p);
}

// NOLINTEND(cppcoreguidelines-no-malloc)
