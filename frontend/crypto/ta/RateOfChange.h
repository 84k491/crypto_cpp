#pragma once

#include <chrono>
#include <list>
#include <optional>
#include <utility>

class RateOfChange
{
public:
    RateOfChange(std::chrono::milliseconds interval);
    std::optional<double> push_value(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    std::list<std::pair<std::chrono::milliseconds, double>> m_data;
    std::chrono::milliseconds m_interval;
};
