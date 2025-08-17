#pragma once

#include "JsonStrategyConfig.h"
#include "StrategyBase.h"

struct StaticGridWithBanStrategyConfig
{
    StaticGridWithBanStrategyConfig(JsonStrategyConfig);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_timeframe = {};
    size_t m_interval = 0;
    double m_level_width_perc = 0;
};

class StaticGridWithBan : public StrategyBase
{
    struct Limits
    {
        double max_price = 0.;
        double mid_price() const;
        double min_price = std::numeric_limits<double>::max();

        double price_radius() const;
        void update_prices(double price);
    };

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

public:
    StaticGridWithBan(
            const StaticGridWithBanStrategyConfig & config,
            EventLoopSubscriber & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool export_price_levels() const override { return false; }
    bool is_valid() const override;
    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    virtual bool is_banned(std::chrono::milliseconds ts, const Candle & candle) = 0;

    void push_candle(std::chrono::milliseconds ts, const Candle & candle);

    void try_interval_handover(std::chrono::milliseconds ts);

    void on_order_traded(const MarketOrder & order, int price_level);
    void on_take_profit_traded(const TakeProfitMarketOrder & ev, int price_level);
    void on_stop_loss_traded(const StopLossMarketOrder & ev, int price_level);

    struct TpSlPrices
    {
        double take_profit_price = 0;
        double stop_loss_price = 0;
    };
    TpSlPrices calc_tp_sl_prices(double order_price, Side side) const;

private:
    int get_level_number(double price) const;
    double get_price_from_level_number(int level_num) const;
    int get_levels_per_side() const;
    double level_width_in_currency() const;

    void report_levels(std::chrono::milliseconds ts);
    // void clear_levels(std::chrono::milliseconds ts);
    // void print_levels();

private:
    EventLoopSubscriber & m_event_loop;
    StaticGridWithBanStrategyConfig m_config;
    OrderManager & m_orders;

    std::optional<Limits> m_previous_limits;
    Limits m_next_limits;

    std::chrono::milliseconds m_current_interval_start_ts = {};

    std::map<int, Level> m_orders_by_levels;

    int m_max_levels_per_side = 0;
    bool m_prev_banned_state = false;
};
