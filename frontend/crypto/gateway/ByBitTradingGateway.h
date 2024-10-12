#pragma once

#include "ByBitTradingMessages.h"
#include "ConnectionWatcher.h"
#include "EventLoop.h"
#include "Events.h"
#include "GatewayConfig.h"
#include "Guarded.h"
#include "ITradingGateway.h"
#include "RestClient.h"
#include "WebSocketClient.h"

#include <string>

class ByBitTradingGateway final
    : public ITradingGateway
    , public IConnectionSupervisor
    , private IEventInvoker<OrderRequestEvent, TpslRequestEvent, TrailingStopLossRequestEvent, PingCheckEvent>
{
    static constexpr std::chrono::seconds ws_ping_interval = std::chrono::seconds(5);

public:
    ByBitTradingGateway();

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;
    void push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev) override;

    void register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers) override;
    void unregister_consumers(xg::Guid guid) override;

private:
    bool check_consumers(const std::string & symbol);

    void invoke(const std::variant<OrderRequestEvent, TpslRequestEvent, TrailingStopLossRequestEvent, PingCheckEvent> & value) override;
    void process_event(const OrderRequestEvent & order);
    void process_event(const TpslRequestEvent & tpsl);
    void process_event(const TrailingStopLossRequestEvent & tsl);
    void process_event(const PingCheckEvent & ping_event);

    bool reconnect_ws_client();

    void on_ws_message(const json & j);
    void on_order_response(const json & j);
    void on_execution(const json & j);

    // IConnectionSupervisor
    void on_connection_lost() override;
    void on_connection_verified() override;

private:
    GatewayConfig::Trading m_config;

    RestClient rest_client;
    std::shared_ptr<WebSocketClient> m_ws_client;
    ConnectionWatcher m_connection_watcher;

    Guarded<std::map<std::string, std::pair<xg::Guid, TradingGatewayConsumers>>> m_consumers;

    std::shared_ptr<EventLoop<OrderRequestEvent, TpslRequestEvent, TrailingStopLossRequestEvent, PingCheckEvent>> m_event_loop;
};
