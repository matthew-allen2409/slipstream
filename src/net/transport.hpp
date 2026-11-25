#pragma once

/// @file
/// @brief Market-data transport and handler interfaces.

#include "codec/messages.hpp"

namespace slipstream::net {

/// @brief Receives decoded market-data messages.
///
/// Virtual dispatch here (once per message) instead of std::function avoids a type-erased callable
/// that may heap-allocate.
class IMdHandler {
public:
    IMdHandler() = default;
    IMdHandler(const IMdHandler&) = delete;
    IMdHandler& operator=(const IMdHandler&) = delete;
    IMdHandler(IMdHandler&&) = delete;
    IMdHandler& operator=(IMdHandler&&) = delete;
    virtual ~IMdHandler() = default;

    /// @brief Called for each Quote received.
    virtual void on_quote(const codec::Quote& q) = 0;
    /// @brief Called for each Trade received.
    virtual void on_trade(const codec::Trade& t) = 0;
    /// @brief Called for each Heartbeat received.
    virtual void on_heartbeat(const codec::Heartbeat& h) = 0;
    /// @brief Called for each SessionControl received.
    virtual void on_session_control(const codec::SessionControl& s) = 0;
};

/// @brief Swappable market-data transport (TCP now; UDP multicast or gRPC in M4).
///
/// The engine must not change when the transport does.
class IFeedTransport {
public:
    IFeedTransport() = default;
    IFeedTransport(const IFeedTransport&) = delete;
    IFeedTransport& operator=(const IFeedTransport&) = delete;
    IFeedTransport(IFeedTransport&&) = delete;
    IFeedTransport& operator=(IFeedTransport&&) = delete;
    virtual ~IFeedTransport() = default;

    /// @brief Starts delivering messages to @p handler, which must outlive the transport.
    virtual void start(IMdHandler& handler) = 0;
    /// @brief Stops delivering messages.
    virtual void stop() = 0;
};

}  // namespace slipstream::net
