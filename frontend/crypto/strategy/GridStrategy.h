#pragma once

#include "ConditionalOrders.h"
#include "EventLoopSubscriber.h"
#include "JsonStrategyConfig.h"
#include "OrderManager.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"
#include "TimeWeightedMovingAverage.h"

struct GridStrategyConfig
{
    GridStrategyConfig(JsonStrategyConfig);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    double get_one_level_width(double ref_price) const;

    std::chrono::milliseconds m_timeframe = {};
    std::chrono::milliseconds m_interval = {};
    unsigned m_levels_per_side = 0;
    double m_price_radius_perc = 0.; // to the end of the last level
};

namespace GridLevels {

int get_level_number(double price, double trend, double level_width);
double get_price_from_level_number(int level_num, double trend, double level_width);

} // namespace GridLevels

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

    void on_order_traded(const MarketOrder & order, int price_level);

    void on_take_profit_traded(const TakeProfitMarketOrder & ev, int price_level);
    void on_stop_loss_traded(const StopLossMarketOrder & ev, int price_level);

    struct TpSlPrices
    {
        double take_profit_price = 0;
        double stop_loss_price = 0;
    };
    TpSlPrices calc_tp_sl_prices(double order_price, Side side) const;

    struct Level
    {
        int level_num = 0;

        std::shared_ptr<ISubscription> mo_sub;
        std::shared_ptr<MarketOrder> market_order;

        std::shared_ptr<ISubscription> tp_sub;
        std::shared_ptr<TakeProfitMarketOrder> tp;

        std::shared_ptr<ISubscription> sl_sub;
        std::shared_ptr<StopLossMarketOrder> sl;
    };

    int get_level_number(double price) const;
    double get_price_from_level_number(int level_num) const;

private:
    void maybe_report_levels(std::chrono::milliseconds ts);
    void print_levels();
    std::chrono::milliseconds last_reported_ts = {};

private:
    EventLoopSubscriber & m_event_loop;

    GridStrategyConfig m_config;

    OrderManager & m_orders;

    TimeWeightedMovingAverage m_trend;
    double m_last_trend_value = 0.;

    std::map<int, Level> m_orders_by_levels;
};
