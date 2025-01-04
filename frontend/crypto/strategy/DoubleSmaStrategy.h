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
    DoubleSmaStrategyConfig(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval);

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

    std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>>
    get_internal_data_history() const;

private:
    const DoubleSmaStrategyConfig m_config;

    MovingAverage m_slow_avg;
    MovingAverage m_fast_avg;

    std::vector<std::pair<std::chrono::milliseconds, double>> m_slow_avg_history;
    std::vector<std::pair<std::chrono::milliseconds, double>> m_fast_avg_history;

    std::optional<double> m_prev_slow_avg;
    std::optional<double> m_prev_fast_avg;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
