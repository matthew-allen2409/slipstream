#pragma once

#include <cstddef>

namespace slipstream::test {

// Counts global operator new calls made on the current thread while the guard is alive.
// Used to assert the hot path allocates nothing:
//
//     AllocGuard guard;
//     run_hot_path();
//     EXPECT_EQ(guard.allocations(), 0u);
//
// Read allocations() into a local before handing it to an EXPECT_* macro, so the assertion
// machinery is never counted.
class AllocGuard {
public:
    AllocGuard() noexcept;
    AllocGuard(const AllocGuard&) = delete;
    AllocGuard& operator=(const AllocGuard&) = delete;
    AllocGuard(AllocGuard&&) = delete;
    AllocGuard& operator=(AllocGuard&&) = delete;
    ~AllocGuard() = default;

    [[nodiscard]] std::size_t allocations() const noexcept;
    [[nodiscard]] std::size_t bytes() const noexcept;

private:
    std::size_t start_count_;
    std::size_t start_bytes_;
};

// Keeps the optimiser from eliding an allocation (new/delete pairs may legally be removed).
inline void escape(const void* p) noexcept { asm volatile("" : : "g"(p) : "memory"); }

}  // namespace slipstream::test
