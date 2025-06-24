#pragma once

#include "ConditionalOrders.h"
#include "EventLoopSubscriber.h"
#include "ITradingGateway.h"
#include "MarketOrder.h"
#include "Symbol.h"
#include "crossguid/guid.hpp"

#include <chrono>
#include <map>

class OrderManager
{
public:
    using OrderCallback = std::function<void(const MarketOrder &, bool)>;
    using TakeProfitCallback = std::function<void(const TakeProfitMarketOrder &, bool)>;
    using StopLossCallback = std::function<void(const StopLossMarketOrder &, bool)>;

    OrderManager(
            Symbol symbol,
            EventLoopSubscriber & event_loop,
            ITradingGateway & tr_gateway);

    void send_market_order(double price, SignedVolume vol, std::chrono::milliseconds ts, OrderCallback && on_response);
    const std::map<xg::Guid, std::pair<MarketOrder, OrderCallback>> & pending_orders() const;

    void send_take_profit(double price, SignedVolume vol, std::chrono::milliseconds ts, TakeProfitCallback && on_response);
    void send_stop_loss(double price, SignedVolume vol, std::chrono::milliseconds ts, StopLossCallback && on_response);

private:
    std::variant<SignedVolume, std::string> adjusted_volume(SignedVolume vol);

    void on_order_response(const OrderResponseEvent & r);
    void on_take_profit_response(const TakeProfitUpdatedEvent & r);
    void on_stop_loss_reposnse(const StopLossUpdatedEvent & r);

private:
    Symbol m_symbol;
    ITradingGateway & m_tr_gateway;

    std::map<xg::Guid, std::pair<MarketOrder, OrderCallback>> m_pending_orders;
    std::map<xg::Guid, std::pair<TakeProfitMarketOrder, TakeProfitCallback>> m_pending_tp;

    EventLoopSubscriber & m_event_loop;
    EventChannel<std::string> m_error_channel;
};
