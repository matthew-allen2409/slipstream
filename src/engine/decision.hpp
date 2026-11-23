#pragma once

#include <cstdint>

namespace slipstream::engine {

// Why a trade was rejected. A rejection is a normal business outcome, not an error, so it is a
// value in Decision rather than the error branch of a std::expected.
enum class RejectReason : std::uint8_t {
    None,
    WarmUp,         // < one full window of feed time, or < 10 quotes observed
    Halted,         // SessionControl HALT in effect
    NoBook,         // no L1 quote for the symbol yet
    Band,           // quote not cheap/rich enough vs VWAP, or tpx did not cross the quote
    MaxQuantity,    // qty > --max-quantity
    Participation,  // would breach --participation-cap
};

// GCOE/1 ExecReport reason_code: 0=none, 1=risk, 2=price, 3=size, 4=throttle.
[[nodiscard]] constexpr std::uint8_t to_reason_code(RejectReason r) noexcept {
    switch (r) {
        case RejectReason::None: return 0;
        case RejectReason::WarmUp:
        case RejectReason::Halted:
        case RejectReason::NoBook:
        case RejectReason::Participation: return 1;
        case RejectReason::Band: return 2;
        case RejectReason::MaxQuantity: return 3;
    }
    return 0;
}

struct Decision {
    RejectReason reason = RejectReason::None;

    [[nodiscard]] constexpr bool accepted() const noexcept { return reason == RejectReason::None; }
};

}  // namespace slipstream::engine
