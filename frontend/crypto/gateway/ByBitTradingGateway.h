#pragma once

#include "ByBitTradingMessages.h"
#include "EventLoop.h"
#include "Events.h"
#include "Guarded.h"
#include "ITradingGateway.h"
#include "MarketOrder.h"
#include "RestClient.h"
#include "WebSocketClient.h"

#include <string>

struct PendingOrder
{
    PendingOrder(const MarketOrder & order)
        : m_order(order)
    {
    }
    MarketOrder m_order;
    std::promise<bool> success_promise;
};

class ByBitTradingGateway final
    : public ITradingGateway
    , private IEventInvoker<OrderRequestEvent, TpslRequestEvent>
{
    static constexpr std::string_view s_rest_base_url = "https://api-testnet.bybit.com";
    static constexpr std::chrono::seconds ws_wait_timeout = std::chrono::seconds(5);

public:
    ByBitTradingGateway();

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;

    void register_trade_consumer(xg::Guid guid, const Symbol & symbol, IEventConsumer<TradeEvent> & consumer) override;
    void unregister_trade_consumer(xg::Guid guid) override;
    bool check_trade_consumer(const std::string & symbol);

private:
    void invoke(const std::variant<OrderRequestEvent, TpslRequestEvent> & value) override;
    void process_event(const OrderRequestEvent & order);
    void process_event(const TpslRequestEvent & tpsl);

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

    Guarded<std::map<std::string, std::pair<xg::Guid, IEventConsumer<TradeEvent> *>>> m_trade_consumers;
};
