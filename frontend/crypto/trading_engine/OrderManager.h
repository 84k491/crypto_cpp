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

// TODO handle order rejects
class OrderManager
{
public:
    OrderManager(
            Symbol symbol,
            EventLoopSubscriber & event_loop,
            ITradingGateway & tr_gateway);

    EventObjectChannel<std::shared_ptr<MarketOrder>> & send_market_order(double price, SignedVolume vol, std::chrono::milliseconds ts);
    const auto & pending_orders() const { return m_orders; }
    size_t conditionals() const { return m_take_profits.size() + m_stop_losses.size(); }

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
    auto & error_channel() { return m_error_channel; }

private:
    struct MarketOrderChannelWithAckInfo
    {
        EventObjectChannel<std::shared_ptr<MarketOrder>> ch;
        bool acked = false;
        bool traded = false;
    };

    std::variant<SignedVolume, std::string> adjusted_volume(SignedVolume vol);

    void on_order_response(const OrderResponseEvent & r);
    void on_take_profit_response(const TakeProfitUpdatedEvent & r);
    void on_stop_loss_reposnse(const StopLossUpdatedEvent & r);

    void on_trade(const TradeEvent & ev);
    bool try_trade_market_order(const TradeEvent & ev);
    bool try_trade_take_profit(const TradeEvent & ev);
    bool try_trade_stop_loss(const TradeEvent & ev);

private:
    Symbol m_symbol;
    ITradingGateway & m_tr_gateway;

    std::map<xg::Guid, MarketOrderChannelWithAckInfo> m_orders;
    std::map<xg::Guid, EventObjectChannel<std::shared_ptr<TakeProfitMarketOrder>>> m_take_profits;
    std::map<xg::Guid, EventObjectChannel<std::shared_ptr<StopLossMarketOrder>>> m_stop_losses;

    EventLoopSubscriber & m_event_loop;
    EventChannel<std::string> m_error_channel;
};
