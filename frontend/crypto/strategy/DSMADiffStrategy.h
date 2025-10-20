#pragma once

#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"
#include "TimeWeightedMovingAverage.h"

class DSMADiffStrategyConfig
{
public:
    DSMADiffStrategyConfig(const JsonStrategyConfig & json);
    DSMADiffStrategyConfig(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    std::chrono::milliseconds m_slow_interval = {};
    std::chrono::milliseconds m_fast_interval = {};
    double m_diff_threshold_percent = {};

    // exit
    double m_risk;
    double m_no_loss_coef;
};

class DSMADiffStrategy final : public StrategyBase
{
public:
    DSMADiffStrategy(
            const DSMADiffStrategyConfig & conf,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override { return {}; }

private:
    void push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    const DSMADiffStrategyConfig m_config;
    DynamicTrailingStopLossStrategy m_exit_strategy;

    TimeWeightedMovingAverage m_slow_avg;
    TimeWeightedMovingAverage m_fast_avg;
    double m_diff_threshold = {}; // coef, not percent

    EventSubcriber m_sub;
};
