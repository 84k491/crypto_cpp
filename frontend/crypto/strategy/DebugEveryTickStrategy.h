#pragma once

#include "JsonStrategyConfig.h"
#include "Signal.h"
#include "StrategyInterface.h"

#include <chrono>
#include <optional>

class DebugEveryTickStrategyConfig
{
public:
    DebugEveryTickStrategyConfig(const JsonStrategyConfig & json);
    DebugEveryTickStrategyConfig(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval);

    static bool is_valid();

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_slow_interval = {};
    std::chrono::milliseconds m_fast_interval = {};
};

class DebugEveryTickStrategy final : public IStrategy
{
    static constexpr unsigned max_iter = 10;

public:
    using ConfigT = DebugEveryTickStrategyConfig;

    DebugEveryTickStrategy(const DebugEveryTickStrategyConfig & conf);
    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override;
    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;
    bool is_valid() const override;
    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    const DebugEveryTickStrategyConfig m_config;

    Side last_side = Side::sell();

    unsigned iteration = 0;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
