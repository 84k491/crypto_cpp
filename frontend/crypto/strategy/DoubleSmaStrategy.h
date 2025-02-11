#pragma once

#include "JsonStrategyConfig.h"
#include "Signal.h"
#include "SimpleMovingAverage.h"
#include "StrategyInterface.h"

#include <chrono>
#include <map>
#include <optional>
#include <vector>

class DoubleSmaStrategyConfig
{
public:
    DoubleSmaStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_slow_interval = {};
    std::chrono::milliseconds m_fast_interval = {};
};

class DoubleSmaStrategy final : public IStrategy
{
public:
    using ConfigT = DoubleSmaStrategyConfig;

    DoubleSmaStrategy(const DoubleSmaStrategyConfig & conf);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    const DoubleSmaStrategyConfig m_config;

    SimpleMovingAverage m_slow_avg;
    SimpleMovingAverage m_fast_avg;

    std::optional<double> m_prev_slow_avg;
    std::optional<double> m_prev_fast_avg;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
