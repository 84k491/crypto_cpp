#pragma once

#include "Signal.h"
#include "nlohmann/json.hpp"
#include "SimpleMovingAverage.h"

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <vector>

class DoubleSmaStrategyConfig
{
public:
    DoubleSmaStrategyConfig(const nlohmann::json& json);
    DoubleSmaStrategyConfig(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval);

    bool is_valid() const;

    nlohmann::json to_json() const;

    std::chrono::milliseconds m_slow_interval = {};
    std::chrono::milliseconds m_fast_interval = {};
};

class DoubleSmaStrategy
{
public:
    using ConfigT = DoubleSmaStrategyConfig;

    DoubleSmaStrategy(const DoubleSmaStrategyConfig & conf);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

    std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>>
    get_internal_data_history() const;

    void subscribe_for_strategy_internal(std::function<void(std::string name,
                                                            std::chrono::milliseconds ts,
                                                            double data)> && cb);

private:
    const DoubleSmaStrategyConfig m_config;

    MovingAverage m_slow_avg;
    MovingAverage m_fast_avg;

    std::vector<std::pair<std::chrono::milliseconds, double>> m_slow_avg_history;
    std::vector<std::pair<std::chrono::milliseconds, double>> m_fast_avg_history;

    std::optional<double> m_prev_slow_avg{};
    std::optional<double> m_prev_fast_avg{};

    std::vector<std::function<void(std::string name,
                                   std::chrono::milliseconds ts,
                                   double data)>>
            m_strategy_internal_callbacks;
};
