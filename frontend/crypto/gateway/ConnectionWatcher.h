#pragma once

#include "Events.h"

class IPingSender
{
public:
    virtual ~IPingSender() = default;
    virtual void send_ping() = 0;
    virtual void on_connection_lost() = 0;
};

class ConnectionWatcher
{
public:
    ConnectionWatcher(IPingSender & ping_sender);

    void on_pong_received();
    // TODO use some method like "on_connection_verified()" ?
    [[nodiscard("Check if connection is lost")]] bool handle_request(const PingCheckEvent & request);

public:
    IPingSender & m_ping_sender;
    bool m_pong_received = false;
};
