#pragma once

#include "ConditionalOrders.h"
#include "EventLoopSubscriber.h"
#include "EventObjectChannel.h"
#include "ITradingGateway.h"
#include "MarketOrder.h"
#include "Symbol.h"
#include "crossguid/guid.hpp"

#include <chrono>
#include <map>

class OrderManager
{
public:
    using TakeProfitCallback = std::function<void(const TakeProfitMarketOrder &, bool)>;
    using StopLossCallback = std::function<void(const StopLossMarketOrder &, bool)>;

    OrderManager(
            Symbol symbol,
            EventLoopSubscriber & event_loop,
            ITradingGateway & tr_gateway);

    EventObjectChannel<std::shared_ptr<MarketOrder>> & send_market_order(double price, SignedVolume vol, std::chrono::milliseconds ts);
    const auto & pending_orders() const { return m_orders; }

    [[nodiscard("Subscribe for the channel")]]
    EventObjectChannel<std::shared_ptr<TakeProfitMarketOrder>> & send_take_profit(
            double price,
            SignedVolume vol,
            std::chrono::milliseconds ts);

    [[nodiscard("Subscribe for the channel")]]
    EventObjectChannel<std::shared_ptr<StopLossMarketOrder>> & send_stop_loss(
            double price,
            SignedVolume vol,
            std::chrono::milliseconds ts);

    void cancel_take_profit(xg::Guid);
    void cancel_stop_loss(xg::Guid);

private:
    std::variant<SignedVolume, std::string> adjusted_volume(SignedVolume vol);

    void on_order_response(const OrderResponseEvent & r);
    void on_order_trade(const TradeEvent & ev);
    void on_take_profit_response(const TakeProfitUpdatedEvent & r);
    void on_stop_loss_reposnse(const StopLossUpdatedEvent & r);

private:
    Symbol m_symbol;
    ITradingGateway & m_tr_gateway;

    std::map<xg::Guid, EventObjectChannel<std::shared_ptr<MarketOrder>>> m_orders;
    std::map<xg::Guid, EventObjectChannel<std::shared_ptr<TakeProfitMarketOrder>>> m_pending_tp;
    std::map<xg::Guid, EventObjectChannel<std::shared_ptr<StopLossMarketOrder>>> m_pending_sl;

    EventLoopSubscriber & m_event_loop;
    EventChannel<std::string> m_error_channel;
};
