#pragma once

#include "ByBitTradingMessages.h"
#include "ConnectionWatcher.h"
#include "EventChannel.h"
#include "EventLoop.h"
#include "Events.h"
#include "GatewayConfig.h"
#include "ITradingGateway.h"
#include "RestClient.h"
#include "WebSocketClient.h"

class ByBitTradingGateway final
    : public ITradingGateway
    , public IConnectionSupervisor
{
    static constexpr std::chrono::seconds ws_ping_interval = std::chrono::seconds(5);

public:
    ByBitTradingGateway();

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;
    void push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev) override;
    void push_take_profit_request(const TakeProfitMarketOrder &) override;
    void push_stop_loss_request(const StopLossMarketOrder &) override;
    void cancel_stop_loss_request(xg::Guid guid) override;
    void cancel_take_profit_request(xg::Guid guid) override;

    EventChannel<OrderResponseEvent> & order_response_channel() override;
    EventChannel<TradeEvent> & trade_channel() override;
    EventChannel<TpslResponseEvent> & tpsl_response_channel() override;
    EventChannel<TpslUpdatedEvent> & tpsl_updated_channel() override;
    EventChannel<TrailingStopLossResponseEvent> & trailing_stop_response_channel() override;
    EventChannel<TrailingStopLossUpdatedEvent> & trailing_stop_update_channel() override;
    EventChannel<StopLossUpdatedEvent> & stop_loss_update_channel() override;
    EventChannel<TakeProfitUpdatedEvent> & take_profit_update_channel() override;

private:
    void register_subs();
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

    EventLoopSubscriber m_event_loop;

    EventChannel<OrderRequestEvent> m_order_req_channel;
    EventChannel<TpslRequestEvent> m_tpsl_req_channel;
    EventChannel<TrailingStopLossRequestEvent> m_tsl_req_channel;
    EventChannel<PingCheckEvent> m_ping_event_channel;

    EventChannel<OrderResponseEvent> m_order_response_channel;
    EventChannel<TradeEvent> m_trade_channel;
    EventChannel<TpslResponseEvent> m_tpsl_response_channel;
    EventChannel<TpslUpdatedEvent> m_tpsl_updated_channel;
    EventChannel<TrailingStopLossResponseEvent> m_trailing_stop_response_channel;
    EventChannel<TrailingStopLossUpdatedEvent> m_trailing_stop_update_channel;
};
