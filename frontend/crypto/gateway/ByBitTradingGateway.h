#pragma once

#include "ByBitTradingMessages.h"
#include "EventLoop.h"
#include "Events.h"
#include "Guarded.h"
#include "ITradingGateway.h"
#include "RestClient.h"
#include "WebSocketClient.h"

#include <string>

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

    void register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers) override;
    void unregister_consumers(xg::Guid guid) override;

private:
    bool check_consumers(const std::string & symbol);

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

    Guarded<std::map<std::string, std::pair<xg::Guid, TradingGatewayConsumers *>>> m_consumers;
};
