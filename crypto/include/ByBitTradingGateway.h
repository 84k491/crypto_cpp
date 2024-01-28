#pragma once

#include "Position.h"
#include "RestClient.h"
#include "ByBitTradingMessages.h"
#include "WorkerThread.h"

#include <memory>
#include <string>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/roles/client_endpoint.hpp>

struct PendingOrder
{
    MarketOrder order;
    std::promise<ByBitMessages::OrderResponse> order_resp_promise;
    std::promise<ByBitMessages::Execution> exec_promise;
};

class ByBitTradingGateway
{
    using WsConfigClient = websocketpp::config::asio_tls;
    using WsClient = websocketpp::client<WsConfigClient>;
    using message_ptr = WsConfigClient::message_type::ptr;
    using context_ptr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;

    static constexpr std::chrono::seconds ws_wait_timeout = std::chrono::seconds(5);

public:
    ByBitTradingGateway();

    std::optional<ByBitMessages::Execution> send_order_sync(const MarketOrder & order);

private:
    static std::string sign_message(const std::string & message, const std::string & secret);
    std::string build_auth_message() const;

    void subscribe();
    void on_ws_message_received(const std::string & message);
    void on_auth_response(const json & j);
    void on_ping_response(const json & j);
    void on_order_response(const json & j);
    void on_sub_response(const json & j);
    void on_execution(const json & j);

    void send_ping();

private:
    std::string m_url;
    std::string m_api_key;
    std::string m_secret_key;

    RestClient rest_client;
    WsClient m_client;
    WsClient::connection_ptr m_connection;

    std::unique_ptr<WorkerThreadLoop> m_ping_worker;

    std::mutex m_pending_orders_mutex;
    std::map<std::string, PendingOrder> m_pending_orders;
};
