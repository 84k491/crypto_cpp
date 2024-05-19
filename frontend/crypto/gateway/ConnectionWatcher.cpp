#include "ConnectionWatcher.h"

ConnectionWatcher::ConnectionWatcher(IPingSender & ping_sender)
    : m_ping_sender(ping_sender)
{
}

void ConnectionWatcher::on_pong_received()
{
    m_pong_received = true;
}

bool ConnectionWatcher::handle_request(const PingCheckEvent &)
{
    if (!m_pong_received) {
        m_ping_sender.on_connection_lost();
        return false;
    }
    m_ping_sender.send_ping();

    return true;
}
