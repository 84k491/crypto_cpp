#pragma once

#include "ByBitTradingMessages.h"
#include "EventLoop.h"
#include "ITradingGateway.h"
#include "MarketOrder.h"
#include "RestClient.h"
#include "WorkerThread.h"
#include "Events.h"

#include <memory>
#include <string>
#include <vector>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/roles/client_endpoint.hpp>

struct PendingOrder
{
    PendingOrder(const MarketOrder & order)
        : m_order(order)
        , m_volume_to_fill(order.volume().value())
    {
    }
    MarketOrder m_order;
    std::promise<bool> success_promise;

    double m_volume_to_fill;
    bool m_acked = false;
    std::vector<Trade> m_trades;
};

class ByBitTradingGateway final
    : public ITradingGateway
    , private IEventInvoker<OrderRequestEvent>
{
    using WsConfigClient = websocketpp::config::asio_tls;
    using WsClient = websocketpp::client<WsConfigClient>;
    using message_ptr = WsConfigClient::message_type::ptr;
    using context_ptr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;

    static constexpr std::chrono::seconds ws_wait_timeout = std::chrono::seconds(5);

public:
    ByBitTradingGateway();

    void push_order_request(const OrderRequestEvent & order) override;

private:
    void invoke(const std::variant<OrderRequestEvent> & value) override;

    static std::string sign_message(const std::string & message, const std::string & secret);
    std::string build_auth_message() const;

    void subscribe();
    void on_ws_message_received(const std::string & message);
    void on_auth_response(const json & j);
    void on_ping_response(const json & j);
    void on_sub_response(const json & j);

    void on_order_response(const json & j);
    void on_execution(const json & j);

    void send_ping();

private:
    EventLoop<OrderRequestEvent> m_event_loop;

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
