#include "StandardDeviation.h"
#include <cmath>

std::optional<double> StandardDeviation::push_value(std::pair<std::chrono::milliseconds, double> ts_and_value)
{
    const auto & [ts, value] = ts_and_value;
    m_values.push_back(ts_and_value);
    m_sum_for_average += value;
    const auto average = m_sum_for_average / static_cast<double>(m_values.size());

    const auto deviation = value - average;
    m_deviations.push_back(deviation);
    m_sum_of_deviations += deviation;
    if ((m_values.back().first - m_values.front().first) < m_interval) {
        return std::nullopt;
    }
    m_sum_for_average -= m_values.front().second;
    m_values.pop_front();
    m_sum_of_deviations -= m_deviations.front();
    m_deviations.pop_front();

    return std::sqrt(m_sum_of_deviations / static_cast<double>(m_deviations.size() - 1));
}
