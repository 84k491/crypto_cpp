#pragma once

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

    std::chrono::milliseconds m_slow_interval = {};
    std::chrono::milliseconds m_fast_interval = {};
    double m_diff_threshold_percent = {};
};

class DSMADiffStrategy final : public StrategyBase
{
public:
    DSMADiffStrategy(
            const DSMADiffStrategyConfig & conf,
            EventLoopSubscriber & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override { return {}; }

private:
    void push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    const DSMADiffStrategyConfig m_config;

    TimeWeightedMovingAverage m_slow_avg;
    TimeWeightedMovingAverage m_fast_avg;
    double m_diff_threshold = {}; // coef, not percent
};
