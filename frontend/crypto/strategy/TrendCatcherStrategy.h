#pragma once

#include "EventLoop.h"
#include "JsonStrategyConfig.h"
#include "OrderManager.h"
#include "StandardDeviation.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"
#include "TimeWeightedMovingAverage.h"
#include "TpslExitStrategy.h"

class TrendCatcherStrategyConfig
{
public:
    TrendCatcherStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    std::chrono::milliseconds m_timeframe = {};

    std::chrono::milliseconds m_slow_interval = {};
    std::chrono::milliseconds m_fast_interval = {};

    double m_min_price_diff_perc = 0.;
    double m_max_std_dev_perc = 0.;

    // exit
    double m_risk = 1.;
    double m_risk_reward_ratio = 1.;
};

/*
    two MA over a price diff
    one with bigger interval, one with smaller

    two std dev indicators, with same intervals

    signal when both MAs show same trend direction (higher than minimum)
    and std dev is below maximum

    tpsl exit for now
*/
class TrendCatcherStrategy : public StrategyBase
{
public:
    TrendCatcherStrategy(
            const TrendCatcherStrategyConfig & config,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    void push_candle(const Candle &);

private:
    TrendCatcherStrategyConfig m_config;

    TimeWeightedMovingAverage m_slow_diff_ma;
    StandardDeviation m_slow_std_dev;

    TimeWeightedMovingAverage m_fast_diff_ma;
    StandardDeviation m_fast_std_dev;

    TpslExitStrategy m_exit;

    EventSubcriber m_sub;
};
