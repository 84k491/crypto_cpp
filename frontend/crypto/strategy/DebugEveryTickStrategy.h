#pragma once

#include "JsonStrategyConfig.h"
#include "Signal.h"
#include "SimpleMovingAverage.h"
#include "StrategyInterface.h"

#include "nlohmann/json.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <vector>

class DebugEveryTickStrategyConfig
{
public:
    DebugEveryTickStrategyConfig(const JsonStrategyConfig & json);
    DebugEveryTickStrategyConfig(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval);

    bool is_valid() const;

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
    TimeseriesPublisher<std::pair<std::string, double>> & strategy_internal_data_publisher() override;
    bool is_valid() const override;

private:
    const DebugEveryTickStrategyConfig m_config;

    Side last_side = Side::Sell;

    unsigned iteration = 0;

    TimeseriesPublisher<std::pair<std::string, double>> m_strategy_internal_data_publisher;
};
