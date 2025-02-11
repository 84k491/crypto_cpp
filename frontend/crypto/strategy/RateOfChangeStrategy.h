#pragma once

#include "JsonStrategyConfig.h"
#include "RateOfChange.h"
#include "SimpleMovingAverage.h"
#include "StrategyInterface.h"

class RateOfChangeStrategyConfig
{
public:
    RateOfChangeStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_sma_interval = {};
    std::chrono::milliseconds m_trigger_interval = {};
    std::chrono::milliseconds m_roc_interval = {};
    double m_signal_threshold = 0.;
};

class RateOfChangeStrategy : public IStrategy
{
public:
    RateOfChangeStrategy(const RateOfChangeStrategyConfig & config);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override;
    std::optional<Signal> push_candle(const Candle &) override { return {}; }
    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    RateOfChangeStrategyConfig m_config;
    RateOfChange m_rate_of_change;
    SimpleMovingAverage m_moving_average;
    std::chrono::milliseconds m_last_below_trigger_ts = {};
    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
