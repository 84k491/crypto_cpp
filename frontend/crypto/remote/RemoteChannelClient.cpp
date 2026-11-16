#include "RemoteChannelClient.h"

#include "RemoteMessagesJsonCodec.h"

#include <stdexcept>

RemoteChannelClient::RemoteChannelClient(const std::string & host, uint16_t port)
{
    boost::asio::ip::tcp::socket socket{m_io_context};
    try {
        socket.connect({boost::asio::ip::make_address(host), port});
    }
    catch (const boost::system::system_error & e) {
        throw std::runtime_error("Can't connect to " + host + ":" + std::to_string(port) + ": " + e.code().message());
    }
    m_connection = std::make_unique<RemoteConnection>(std::move(socket));
    m_reader_thread = std::thread([this] { read_responses(); });
}

RemoteChannelClient::~RemoteChannelClient()
{
    m_connection->shutdown();
    m_reader_thread.join();
}

nlohmann::json RemoteChannelClient::request_throw(const std::string & channel_name, const std::string & operation, nlohmann::json arguments)
{
    return exchange_throw({0, channel_name, operation, std::move(arguments)}, {}).data;
}

uint64_t RemoteChannelClient::subscribe_throw(const std::string & channel_name, nlohmann::json arguments, StreamCallback && stream_callback)
{
    return exchange_throw({0, channel_name, RemoteOperation::subscribe, std::move(arguments)}, std::move(stream_callback)).request_id;
}

void RemoteChannelClient::unsubscribe(const std::string & channel_name, uint64_t subscription_request_id)
{
    RemoteRequest request{0, channel_name, RemoteOperation::unsubscribe, {{"subscription_request_id", subscription_request_id}}};
    {
        std::lock_guard lock(m_mutex);
        m_streams.erase(subscription_request_id);
        request.request_id = ++m_last_request_id;
    }
    m_connection->send(request);
}

void RemoteChannelClient::push_stream_closed_notification(ILambdaAcceptor & consumer, xg::Guid subscriber_guid, Priority priority)
{
    consumer.push(LambdaEvent{
            subscriber_guid,
            [this] {
                std::lock_guard lock(m_mutex);
                m_stream_closed = true;
                m_stream_end_condition.notify_all();
            },
            priority});
}

bool RemoteChannelClient::connection_is_lost()
{
    std::lock_guard lock(m_mutex);
    return m_connection_lost;
}

RemoteStreamEnd RemoteChannelClient::wait_for_stream_end()
{
    std::unique_lock lock(m_mutex);
    m_stream_end_condition.wait(lock, [this] { return m_stream_closed || m_connection_lost; });
    return m_stream_closed ? RemoteStreamEnd::ChannelClosed : RemoteStreamEnd::ConnectionLost;
}

RemoteResponse RemoteChannelClient::exchange_throw(RemoteRequest request, StreamCallback && stream_callback)
{
    std::future<RemoteResponse> response_future;
    {
        std::lock_guard lock(m_mutex);
        if (m_connection_lost) {
            throw std::runtime_error("Connection is lost");
        }
        request.request_id = ++m_last_request_id;
        response_future = m_pending_requests[request.request_id].get_future();
        if (stream_callback) {
            m_streams[request.request_id] = std::move(stream_callback);
        }
    }

    m_connection->send(request);

    const auto response = response_future.get();
    if (response.kind == RemoteResponseKind::error) {
        throw std::runtime_error(response.data.get<std::string>());
    }
    return response;
}

void RemoteChannelClient::read_responses()
{
    while (const auto line = m_connection->read_line()) {
        const auto response_opt = JsonCodec<RemoteResponse>::decode(nlohmann::json::parse(line.value(), nullptr, false));
        if (!response_opt.has_value()) {
            continue;
        }
        handle_response(response_opt.value());
    }
    handle_lost_connection();
}

void RemoteChannelClient::handle_response(const RemoteResponse & response)
{
    std::lock_guard lock(m_mutex);

    const bool is_stream_message =
            response.kind == RemoteResponseKind::snapshot ||
            response.kind == RemoteResponseKind::update ||
            response.kind == RemoteResponseKind::closed;
    if (is_stream_message) {
        const auto it = m_streams.find(response.request_id);
        if (it != m_streams.end()) {
            it->second(response);
        }
        return;
    }

    const auto it = m_pending_requests.find(response.request_id);
    if (it == m_pending_requests.end()) {
        return;
    }
    if (response.kind == RemoteResponseKind::error) {
        m_streams.erase(response.request_id);
    }
    it->second.set_value(response);
    m_pending_requests.erase(it);
}

void RemoteChannelClient::handle_lost_connection()
{
    std::lock_guard lock(m_mutex);
    m_connection_lost = true;
    for (auto & [request_id, response_promise] : m_pending_requests) {
        response_promise.set_exception(std::make_exception_ptr(std::runtime_error("Connection is lost")));
    }
    m_pending_requests.clear();
    m_stream_end_condition.notify_all();
}
