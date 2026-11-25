#pragma once

/// @file
/// @brief The trading decision engine and its output interface.

#include <cstdint>

#include "codec/messages.hpp"
#include "core/symbol.hpp"
#include "engine/decision.hpp"

namespace slipstream::engine {

/// @brief Engine thresholds.
///
/// All thresholds pre-converted to integers at startup: no floating point past argument parsing.
struct EngineConfig {
    core::Symbol symbol;                      ///< Only instrument traded; others are discarded.
    core::Qty max_quantity = 0;               ///< Ceiling on any single trade.
    std::uint32_t participation_cap_ppm = 0;  ///< Parts per million: 0.15 -> 150'000
    std::uint64_t vwap_window_ns = 0;         ///< Rolling VWAP window length.
    std::uint32_t band_tenth_bps = 0;         ///< Tenths of a bp: 25.5 bps -> 255
    std::uint32_t warmup_min_quotes = 10;     ///< Quotes required before trading.
};

/// @brief Receives the engine's outputs.
///
/// Implemented by the transport layer (or a test double), so the engine never sees a socket. That
/// is the dependency inversion the spec asks for.
class IOrderSink {
public:
    IOrderSink() = default;
    IOrderSink(const IOrderSink&) = delete;
    IOrderSink& operator=(const IOrderSink&) = delete;
    IOrderSink(IOrderSink&&) = delete;
    IOrderSink& operator=(IOrderSink&&) = delete;
    virtual ~IOrderSink() = default;

    /// @brief Called once for each order the engine emits.
    virtual void on_new_order(const codec::NewOrder& order) = 0;
};

/// @brief Pure decision engine: L1 book + rolling VWAP + gates.
///
/// No I/O, no allocation after construction.
///
/// TODO(M2/M3): on_quote / on_trade / on_session_control.
class Engine {
public:
    /// @param config Thresholds (copied).
    /// @param sink   Receives emitted orders; must outlive the engine.
    Engine(const EngineConfig& config, IOrderSink& sink) noexcept;

    /// @brief The configuration the engine was constructed with.
    [[nodiscard]] const EngineConfig& config() const noexcept { return config_; }

private:
    EngineConfig config_;
    [[maybe_unused]] IOrderSink* sink_;  // TODO(M3): drop attribute
};

}  // namespace slipstream::engine
