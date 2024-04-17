#pragma once

#include "ByBitTradingMessages.h"
#include "EventLoop.h"
#include "Events.h"
#include "ITradingGateway.h"
#include "MarketOrder.h"
#include "RestClient.h"
#include "WebSocketClient.h"

#include <string>
#include <vector>

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
    , private IEventInvoker<OrderRequestEvent, TpslRequestEvent>
{
    static constexpr std::chrono::seconds ws_wait_timeout = std::chrono::seconds(5);

public:
    ByBitTradingGateway();

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;

private:
    void invoke(const std::variant<OrderRequestEvent, TpslRequestEvent> & value) override;

    void on_ws_message(const json & j);
    void on_order_response(const json & j);
    void on_execution(const json & j);

private:
    EventLoop<OrderRequestEvent, TpslRequestEvent> m_event_loop;

    std::string m_url;
    std::string m_api_key;
    std::string m_secret_key;

    RestClient rest_client;
    WebSocketClient m_ws_client;

    std::mutex m_pending_orders_mutex;
    std::map<std::string, PendingOrder> m_pending_orders;
};
