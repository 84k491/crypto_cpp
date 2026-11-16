#pragma once

#include "ILambdaAcceptor.h"
#include "RemoteConnection.h"
#include "RemoteMessages.h"

#include <boost/asio.hpp>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

enum class RemoteStreamEnd : uint8_t
{
    ChannelClosed,
    ConnectionLost,
};

class RemoteChannelClient
{
public:
    using StreamCallback = std::function<void(const RemoteResponse &)>;

    RemoteChannelClient(const std::string & host, uint16_t port);
    ~RemoteChannelClient();

    nlohmann::json request_throw(const std::string & channel_name, const std::string & operation, nlohmann::json arguments);
    uint64_t subscribe_throw(const std::string & channel_name, nlohmann::json arguments, StreamCallback && stream_callback);
    void unsubscribe(const std::string & channel_name, uint64_t subscription_request_id);

    void push_stream_closed_notification(ILambdaAcceptor & consumer, xg::Guid subscriber_guid, Priority priority);
    RemoteStreamEnd wait_for_stream_end();
    bool connection_is_lost();

private:
    RemoteResponse exchange_throw(RemoteRequest request, StreamCallback && stream_callback);
    void read_responses();
    void handle_response(const RemoteResponse & response);
    void handle_lost_connection();

private:
    std::mutex m_mutex;
    uint64_t m_last_request_id = 0;
    std::map<uint64_t, std::promise<RemoteResponse>> m_pending_requests;
    std::map<uint64_t, StreamCallback> m_streams;

    bool m_connection_lost = false;
    bool m_stream_closed = false;
    std::condition_variable m_stream_end_condition;

    boost::asio::io_context m_io_context;
    std::unique_ptr<RemoteConnection> m_connection;
    std::thread m_reader_thread;
};
