#pragma once

// 128-bit integer aliases for Σ(px × qty)-style accumulators.
//
// __int128 is a GCC/Clang extension; -Wpedantic rejects it unless the declaration is marked
// __extension__. Bound used by the VWAP accumulators: qty < 2^32 and |px| < 2^63, so a single
// px × qty term is < 2^95, leaving 2^32 terms of headroom below 2^127.

namespace slipstream::core {

__extension__ typedef __int128 i128;           // NOLINT(modernize-use-using)
__extension__ typedef unsigned __int128 u128;  // NOLINT(modernize-use-using)

}  // namespace slipstream::core
