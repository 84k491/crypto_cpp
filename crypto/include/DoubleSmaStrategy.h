#pragma once

#include "Signal.h"

#include <chrono>
#include <list>
#include <map>
#include <optional>
#include <vector>

class DoubleSmaStrategyConfig
{
public:
    DoubleSmaStrategyConfig(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval);

    std::chrono::milliseconds m_slow_interval;
    std::chrono::milliseconds m_fast_interval;
};

class DoubleSmaStrategy
{
public:
    DoubleSmaStrategy(const DoubleSmaStrategyConfig & conf);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

    std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>>
    get_internal_data_history() const;

private:
    const DoubleSmaStrategyConfig m_config;

    std::list<std::pair<std::chrono::milliseconds, double>> m_slow_data;
    std::list<std::pair<std::chrono::milliseconds, double>> m_fast_data;

    std::vector<std::pair<std::chrono::milliseconds, double>> m_slow_avg_history;
    std::vector<std::pair<std::chrono::milliseconds, double>> m_fast_avg_history;

    std::optional<double> m_prev_slow_avg{};
    std::optional<double> m_prev_fast_avg{};
};
