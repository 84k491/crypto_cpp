#pragma once

#include "WorkerThread.h"

#include "nlohmann/json_fwd.hpp"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/roles/client_endpoint.hpp>

struct WsKeys
{
    std::string m_api_key;
    std::string m_secret_key;
};

class WebSocketClient
{
    using WsConfigClient = websocketpp::config::asio_tls;
    using WsClient = websocketpp::client<WsConfigClient>;
    using message_ptr = WsConfigClient::message_type::ptr;
    using context_ptr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;
    using json = nlohmann::json;
    using BusinessLogicCallback = std::function<void(const json &)>;

public:
    WebSocketClient(std::string url, std::optional<WsKeys> ws_keys, BusinessLogicCallback callback);
    bool wait_until_ready(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) const;
    void subscribe(const std::string & topic);

private:
    void send_ping();

    static std::string sign_message(const std::string & message, const std::string & secret);
    std::string build_auth_message() const;

private:
    void on_ws_message_received(const std::string & message);
    void on_auth_response(const json & j);
    static void on_ping_response(const json & j);
    void on_sub_response(const json & j);

    void on_connected();

private:
    std::string m_url;
    std::optional<WsKeys> m_keys;

    BusinessLogicCallback m_callback;

    WsClient m_client;
    WsClient::connection_ptr m_connection;

    std::unique_ptr<WorkerThreadLoop> m_ping_worker;

    bool m_ready = false;
};
