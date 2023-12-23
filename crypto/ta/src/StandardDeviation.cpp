#include "StandardDeviation.h"

#include <cmath>

StandardDeviation::StandardDeviation(std::chrono::milliseconds interval)
    : m_interval(interval)
{
}

std::optional<double> StandardDeviation::push_value(std::pair<std::chrono::milliseconds, double> ts_and_value)
{
    const auto & [_, value] = ts_and_value;
    m_values.push_back(ts_and_value);
    m_sum_for_average += value;
    const auto average = m_sum_for_average / static_cast<double>(m_values.size());

    const auto deviation_sq = std::pow(value - average, 2.0);
    m_deviations_sq.push_back(deviation_sq);
    m_sum_of_deviations_sq += deviation_sq;
    if ((m_values.back().first - m_values.front().first) < m_interval) {
        return std::nullopt;
    }
    m_sum_for_average -= m_values.front().second;
    m_values.pop_front();
    m_sum_of_deviations_sq -= m_deviations_sq.front();
    m_deviations_sq.pop_front();

    const auto res = std::sqrt(m_sum_of_deviations_sq / static_cast<double>(m_deviations_sq.size() - 1));
    return res;
}
