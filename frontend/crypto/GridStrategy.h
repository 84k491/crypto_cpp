#pragma once

#include "ConditionalOrders.h"
#include "EventLoopSubscriber.h"
#include "Events.h"
#include "OrderManager.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"
#include "TimeWeightedMovingAverage.h"

class GridStrategyConfig
{
public:
};

class GridStrategy : public StrategyBase
{
public:
    GridStrategy(
            const GridStrategyConfig & config,
            EventLoopSubscriber & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    void push_price(std::chrono::milliseconds ts, double price);
    void on_order_accepted(const MarketOrder & order, int price_level);

    void on_take_profit_accepted(const TakeProfitMarketOrder & tp);
    void on_stop_loss_accepted(const StopLossMarketOrder & sl);

    void on_take_profit_traded(TakeProfitUpdatedEvent ev);
    void on_stop_loss_traded(StopLossUpdatedEvent ev);

private:
    struct Orders
    {
        int level = 0;
        SignedVolume volume;
        std::optional<TakeProfitMarketOrder> tp;
        std::optional<StopLossMarketOrder> sl;
    };

private:
    GridStrategyConfig m_config;

    TimeWeightedMovingAverage m_trend;
    double m_last_trend_value = 0.;

    std::map<int, Orders> m_orders_by_levels;
    // std::map<> m_
};
