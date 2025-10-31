#include "ConnectionWatcher.h"

#include "Logger.h"

ConnectionWatcher::ConnectionWatcher(IConnectionSupervisor & supervisor)
    : m_supervisor(supervisor)
{
}

void ConnectionWatcher::set_ping_sender(std::weak_ptr<IPingSender> & ping_sender)
{
    if (const auto sptr = ping_sender.lock(); sptr) {
        sptr->send_ping();
    }
    else {
        LOG_ERROR("Empty ping sender");
    }
    m_ping_sender = ping_sender;
}

void ConnectionWatcher::on_pong_received()
{
    m_pong_received = true;
}

void ConnectionWatcher::handle_request(const PingCheckEvent &)
{
    if (!m_pong_received) {
        m_supervisor.on_connection_lost();
        return;
    }

    if (const auto sptr = m_ping_sender.lock(); !sptr || !sptr->send_ping()) {
        m_supervisor.on_connection_lost();
        return;
    }

    m_supervisor.on_connection_verified();
    m_pong_received = false;
}
