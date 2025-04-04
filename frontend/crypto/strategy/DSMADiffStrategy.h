#pragma once

#include "EventTimeseriesChannel.h"
#include "JsonStrategyConfig.h"
#include "Signal.h"
#include "SimpleMovingAverage.h"
#include "StrategyInterface.h"
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

class DSMADiffStrategy final : public IStrategy
{
public:
    DSMADiffStrategy(const DSMADiffStrategyConfig & conf);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override;
    std::optional<Signal> push_candle(const Candle &) override { return {}; }

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    const DSMADiffStrategyConfig m_config;

    TimeWeightedMovingAverage m_slow_avg;
    TimeWeightedMovingAverage m_fast_avg;
    double m_diff_threshold = {}; // coef, not percent

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
