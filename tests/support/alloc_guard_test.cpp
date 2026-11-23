#include "support/alloc_guard.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include "perf/latency_recorder.hpp"

namespace {

using slipstream::test::AllocGuard;
using slipstream::test::escape;

TEST(AllocGuard, CountsScalarNew) {
    std::size_t n = 0;
    {
        AllocGuard guard;
        auto p = std::make_unique<int>(42);
        escape(p.get());
        n = guard.allocations();
    }
    EXPECT_EQ(n, 1u);
}

TEST(AllocGuard, CountsVectorGrowth) {
    std::size_t n = 0;
    {
        AllocGuard guard;
        std::vector<int> v;
        for (int i = 0; i < 100; ++i) {
            v.push_back(i);
        }
        escape(v.data());
        n = guard.allocations();
    }
    EXPECT_GT(n, 1u);
}

struct alignas(64) CacheLine {
    std::array<char, 64> bytes;
};

TEST(AllocGuard, CountsAlignedNew) {
    std::size_t n = 0;
    {
        AllocGuard guard;
        auto p = std::make_unique<CacheLine>();
        escape(p.get());
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p.get()) % 64, 0u);
        n = guard.allocations();
    }
    EXPECT_EQ(n, 1u);
}

TEST(AllocGuard, ReportsZeroForStackOnlyWork) {
    std::size_t n = 0;
    {
        AllocGuard guard;
        std::array<int, 256> a{};
        std::iota(a.begin(), a.end(), 0);
        escape(a.data());
        n = guard.allocations();
    }
    EXPECT_EQ(n, 0u);
}

// The pattern every hot-path component will follow: allocate at construction, then prove the
// steady state is allocation-free.
TEST(AllocGuard, LatencyRecorderDoesNotAllocateAfterConstruction) {
    slipstream::perf::LatencyRecorder rec(1024);
    std::size_t n = 0;
    {
        AllocGuard guard;
        for (std::uint64_t i = 0; i < 2048; ++i) {
            rec.record(i);
        }
        n = guard.allocations();
    }
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(rec.size(), 1024u);
    EXPECT_EQ(rec.dropped(), 1024u);
}

}  // namespace
