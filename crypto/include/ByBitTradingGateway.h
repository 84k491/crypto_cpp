#pragma once

#include "Position.h"
#include "RestClient.h"

#include <string>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/roles/client_endpoint.hpp>

class ByBitTradingGateway
{
    using WsConfigClient = websocketpp::config::asio_tls;
    using WsClient = websocketpp::client<WsConfigClient>;
    using message_ptr = WsConfigClient::message_type::ptr;
    using context_ptr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;

public:
    ByBitTradingGateway();

    bool send_order_sync(const MarketOrder & order);

private:
    static std::string sign_message(const std::string & message, const std::string & secret);
    std::string build_auth_message() const;

    void subscribe();
    void on_ws_message_received(const std::string & message);

private:
    std::string m_url;
    std::string m_api_key;
    std::string m_secret_key;

    RestClient rest_client;
    WsClient client;
    WsClient::connection_ptr m_connection;

    std::map<std::string, MarketOrder> m_pending_orders;
};
