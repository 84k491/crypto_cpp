#pragma once

#include "ByBitTradingMessages.h"
#include "ConnectionWatcher.h"
#include "EventLoop.h"
#include "EventChannel.h"
#include "Events.h"
#include "GatewayConfig.h"
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

    EventChannel<OrderResponseEvent> & order_response_channel() override;
    EventChannel<TradeEvent> & trade_channel() override;
    EventChannel<TpslResponseEvent> & tpsl_response_channel() override;
    EventChannel<TpslUpdatedEvent> & tpsl_updated_channel() override;
    EventChannel<TrailingStopLossResponseEvent> & trailing_stop_response_channel() override;
    EventChannel<TrailingStopLossUpdatedEvent> & trailing_stop_update_channel() override;

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

    EventLoopSubscriber<OrderRequestEvent, TpslRequestEvent, TrailingStopLossRequestEvent, PingCheckEvent> m_event_loop;
    EventChannel<OrderResponseEvent> m_order_response_channel;
    EventChannel<TradeEvent> m_trade_channel;
    EventChannel<TpslResponseEvent> m_tpsl_response_channel;
    EventChannel<TpslUpdatedEvent> m_tpsl_updated_channel;
    EventChannel<TrailingStopLossResponseEvent> m_trailing_stop_response_channel;
    EventChannel<TrailingStopLossUpdatedEvent> m_trailing_stop_update_channel;
};
