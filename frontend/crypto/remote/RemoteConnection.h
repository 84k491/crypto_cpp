#pragma once

#include "JsonCodec.h"

#include <boost/asio.hpp>
#include <crossguid/guid.hpp>
#include <mutex>
#include <optional>
#include <string>

class RemoteConnection
{
public:
    explicit RemoteConnection(boost::asio::ip::tcp::socket socket);

    xg::Guid guid() const { return m_guid; }

    template <class MessageT>
    void send(const MessageT & message)
    {
        send_line(JsonCodec<MessageT>::encode(message).dump());
    }

    std::optional<std::string> read_line();
    void shutdown();

private:
    void send_line(const std::string & line);

private:
    const xg::Guid m_guid = xg::newGuid();

    boost::asio::ip::tcp::socket m_socket;
    boost::asio::streambuf m_read_buffer;
    std::mutex m_write_mutex;
};
