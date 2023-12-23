#pragma once

#include <chrono>
#include <list>
#include <optional>

class StandardDeviation
{
public:
    StandardDeviation(std::chrono::milliseconds interval);

    std::optional<double> push_value(std::pair<std::chrono::milliseconds, double> ts_and_value);

private:
    std::list<std::pair<std::chrono::milliseconds, double>> m_values;
    std::chrono::milliseconds m_interval;

    double m_sum_of_deviations_sq = 0.;
    std::list<double> m_deviations_sq;
    double m_sum_for_average = 0.;
};
