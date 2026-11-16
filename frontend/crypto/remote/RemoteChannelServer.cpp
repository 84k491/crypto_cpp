#include "RemoteChannelServer.h"

#include "Logger.h"
#include "RemoteDataDecoding.h"
#include "RemoteMessagesJsonCodec.h"

#include <stdexcept>

RemoteChannelServer::RemoteChannelServer(uint16_t port)
    : m_acceptor(m_io_context)
{
    const boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::address_v4::loopback(), port};

    try {
        m_acceptor = boost::asio::ip::tcp::acceptor{m_io_context, endpoint};
    }
    catch (const boost::system::system_error & e) {
        LOG_ERROR("Remote channel server can't listen on port {}: {}", port, e.code().message());
        return;
    }

    accept_next_connection();
    m_accept_thread = std::thread([this] { m_io_context.run(); });
    LOG_STATUS("Remote channel server listens on port {}", port);
}

RemoteChannelServer::~RemoteChannelServer()
{
    m_io_context.stop();
    if (m_accept_thread.joinable()) {
        m_accept_thread.join();
    }

    for (auto & served_connection : m_connections) {
        served_connection.connection->shutdown();
        served_connection.thread.join();
    }

    std::lock_guard lock(m_channels_mutex);
    m_channels.clear();
}

void RemoteChannelServer::register_channel(const std::string & channel_name, std::unique_ptr<RemoteChannelServerPart> server_part)
{
    std::lock_guard lock(m_channels_mutex);
    m_channels[channel_name] = std::move(server_part);
}

void RemoteChannelServer::unregister_channel(const std::string & channel_name)
{
    std::lock_guard lock(m_channels_mutex);
    m_channels.erase(channel_name);
}

void RemoteChannelServer::accept_next_connection()
{
    m_acceptor.async_accept([this](const boost::system::error_code & error, boost::asio::ip::tcp::socket socket) {
        if (error) {
            return;
        }
        start_serving_connection(std::move(socket));
        accept_next_connection();
    });
}

void RemoteChannelServer::start_serving_connection(boost::asio::ip::tcp::socket socket)
{
    join_finished_connection_threads();

    auto connection = std::make_shared<RemoteConnection>(std::move(socket));
    auto finished = std::make_shared<std::atomic_bool>(false);

    std::lock_guard lock(m_connections_mutex);
    m_connections.push_back({
            connection,
            finished,
            std::thread([this, connection, finished] {
                serve_connection(connection);
                *finished = true;
            }),
    });
}

void RemoteChannelServer::join_finished_connection_threads()
{
    std::lock_guard lock(m_connections_mutex);
    m_connections.remove_if([](ServedConnection & served_connection) {
        if (!*served_connection.finished) {
            return false;
        }
        served_connection.thread.join();
        return true;
    });
}

void RemoteChannelServer::serve_connection(const std::shared_ptr<RemoteConnection> & connection)
{
    while (const auto line = connection->read_line()) {
        handle_request_line(line.value(), connection);
    }
    forget_connection(connection->guid());
}

void RemoteChannelServer::handle_request_line(const std::string & line, const std::shared_ptr<RemoteConnection> & connection)
{
    RemoteRequest request;
    try {
        request = decode_remote_data_throw<RemoteRequest>(nlohmann::json::parse(line));
        handle_request_throw(request, connection);
    }
    catch (const std::exception & e) {
        connection->send(RemoteResponse{request.request_id, RemoteResponseKind::error, e.what()});
    }
}

void RemoteChannelServer::handle_request_throw(const RemoteRequest & request, const std::shared_ptr<RemoteConnection> & connection)
{
    std::lock_guard lock(m_channels_mutex);
    const auto it = m_channels.find(request.channel_name);
    if (it == m_channels.end()) {
        throw std::runtime_error("Unknown channel: " + request.channel_name);
    }
    it->second->handle_request_throw(request, connection);
}

void RemoteChannelServer::forget_connection(xg::Guid connection_guid)
{
    std::lock_guard lock(m_channels_mutex);
    for (auto & [channel_name, server_part] : m_channels) {
        server_part->forget_connection(connection_guid);
    }
}
