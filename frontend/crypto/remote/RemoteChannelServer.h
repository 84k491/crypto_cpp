#pragma once

#include "EventLoop.h"
#include "RemoteChannelServerPart.h"
#include "RemoteConnection.h"

#include <atomic>
#include <boost/asio.hpp>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

class RemoteChannelServer
{
public:
    explicit RemoteChannelServer(uint16_t port);
    ~RemoteChannelServer();

    ILambdaAcceptor & event_loop() { return m_event_loop; }

    void register_channel(const std::string & channel_name, std::unique_ptr<RemoteChannelServerPart> server_part);
    void unregister_channel(const std::string & channel_name);

private:
    void accept_next_connection();
    void start_serving_connection(boost::asio::ip::tcp::socket socket);
    void serve_connection(const std::shared_ptr<RemoteConnection> & connection);
    void handle_request_line(const std::string & line, const std::shared_ptr<RemoteConnection> & connection);
    void handle_request_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection);
    void forget_connection(xg::Guid connection_guid);
    void join_finished_connection_threads();

private:
    struct ServedConnection
    {
        std::shared_ptr<RemoteConnection> connection;
        std::shared_ptr<std::atomic_bool> finished;
        std::thread thread;
    };

    EventLoop m_event_loop;

    std::mutex m_channels_mutex;
    std::map<std::string, std::unique_ptr<RemoteChannelServerPart>> m_channels;

    boost::asio::io_context m_io_context;
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::thread m_accept_thread;

    std::mutex m_connections_mutex;
    std::list<ServedConnection> m_connections;
};
