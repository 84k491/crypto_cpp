#pragma once

#include "Events.h"

class IConnectionSupervisor
{
public:
    virtual ~IConnectionSupervisor() = default;
    virtual void on_connection_lost() = 0;
    virtual void on_connection_verified() = 0;
};

class IPingSender
{
public:
    virtual ~IPingSender() = default;
    virtual bool send_ping() = 0;
};

class ConnectionWatcher
{
public:
    ConnectionWatcher(IConnectionSupervisor & supervisor);

    void set_ping_sender(std::weak_ptr<IPingSender> & ping_sender);
    void on_pong_received();
    void handle_request(const PingCheckEvent &);

public:
    IConnectionSupervisor & m_supervisor;
    std::weak_ptr<IPingSender> m_ping_sender;
    bool m_pong_received = false;
};
