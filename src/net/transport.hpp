#pragma once

#include "codec/messages.hpp"

namespace slipstream::net {

// Receives decoded market-data messages. Virtual dispatch here (once per message) instead of
// std::function avoids a type-erased callable that may heap-allocate.
class IMdHandler {
public:
    IMdHandler() = default;
    IMdHandler(const IMdHandler&) = delete;
    IMdHandler& operator=(const IMdHandler&) = delete;
    IMdHandler(IMdHandler&&) = delete;
    IMdHandler& operator=(IMdHandler&&) = delete;
    virtual ~IMdHandler() = default;

    virtual void on_quote(const codec::Quote& q) = 0;
    virtual void on_trade(const codec::Trade& t) = 0;
    virtual void on_heartbeat(const codec::Heartbeat& h) = 0;
    virtual void on_session_control(const codec::SessionControl& s) = 0;
};

// Swappable transport (TCP now; UDP multicast or gRPC in M4). The engine must not change when
// the transport does.
class IFeedTransport {
public:
    IFeedTransport() = default;
    IFeedTransport(const IFeedTransport&) = delete;
    IFeedTransport& operator=(const IFeedTransport&) = delete;
    IFeedTransport(IFeedTransport&&) = delete;
    IFeedTransport& operator=(IFeedTransport&&) = delete;
    virtual ~IFeedTransport() = default;

    virtual void start(IMdHandler& handler) = 0;
    virtual void stop() = 0;
};

}  // namespace slipstream::net
