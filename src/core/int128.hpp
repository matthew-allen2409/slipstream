#pragma once

/// @file
/// @brief 128-bit integer aliases for sum(px * qty) accumulators.
///
/// __int128 is a GCC/Clang extension; -Wpedantic rejects it unless the declaration is marked
/// __extension__. Bound: qty < 2^32 and |px| < 2^63, so one px * qty term is < 2^95, leaving room
/// for 2^32 terms below 2^127.

namespace slipstream::core {

/// @brief Signed 128-bit integer.
__extension__ typedef __int128 i128;  // NOLINT(modernize-use-using)
/// @brief Unsigned 128-bit integer.
__extension__ typedef unsigned __int128 u128;  // NOLINT(modernize-use-using)

}  // namespace slipstream::core
