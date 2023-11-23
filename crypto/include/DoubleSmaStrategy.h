#pragma once

#include "Signal.h"

#include <chrono>
#include <list>
#include <optional>

class DoubleSmaStrategy
{
public:
    DoubleSmaStrategy(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    std::chrono::milliseconds m_slow_interval{};
    std::chrono::milliseconds m_fast_interval{};

    std::list<std::pair<std::chrono::milliseconds, double>> m_slow_data;
    std::list<std::pair<std::chrono::milliseconds, double>> m_fast_data;

    std::optional<double> m_prev_slow_avg{};
    std::optional<double> m_prev_fast_avg{};
};
