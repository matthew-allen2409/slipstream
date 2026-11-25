#pragma once

/// @file
/// @brief Test helper for asserting that code does not allocate.

#include <cstddef>

namespace slipstream::test {

/// @brief Counts global operator new calls made on the current thread while the guard is alive.
///
/// Used to assert the hot path allocates nothing:
/// @code
///     AllocGuard guard;
///     run_hot_path();
///     EXPECT_EQ(guard.allocations(), 0u);
/// @endcode
///
/// Read allocations() into a local before handing it to an EXPECT_* macro, so the assertion
/// machinery is never counted.
class AllocGuard {
public:
    /// @brief Snapshots the current thread's allocation counters.
    AllocGuard() noexcept;
    AllocGuard(const AllocGuard&) = delete;
    AllocGuard& operator=(const AllocGuard&) = delete;
    AllocGuard(AllocGuard&&) = delete;
    AllocGuard& operator=(AllocGuard&&) = delete;
    ~AllocGuard() = default;

    /// @brief Allocations on this thread since construction.
    [[nodiscard]] std::size_t allocations() const noexcept;
    /// @brief Bytes requested on this thread since construction.
    [[nodiscard]] std::size_t bytes() const noexcept;

private:
    std::size_t start_count_;
    std::size_t start_bytes_;
};

/// @brief Keeps the optimiser from eliding an allocation (new/delete pairs may legally be removed).
inline void escape(const void* p) noexcept { asm volatile("" : : "g"(p) : "memory"); }

}  // namespace slipstream::test
