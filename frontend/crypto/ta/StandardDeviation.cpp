#include "StandardDeviation.h"

#include <cmath>

StandardDeviation::StandardDeviation(std::chrono::milliseconds interval)
    : m_interval(interval)
{
}

std::optional<double> StandardDeviation::push_value(std::pair<std::chrono::milliseconds, double> ts_and_value)
{
    const auto & [ts, value] = ts_and_value;
    m_values.push_back({ts, value});
    m_sum += value;
    m_sq_sum += value * value;

    auto cutoff_ts = ts - m_interval;
    while (!m_values.empty() && m_values.front().ts < cutoff_ts) {
        const auto [_, v] = m_values.front();
        m_sum -= v;
        m_sq_sum -= v * v;
        m_values.pop_front();
    }

    const auto n = m_values.size();
    if (n < 2) {
        return std::nullopt;
    }

    m_mean = m_sum / n;
    double variance = (m_sq_sum / n) - (m_mean * m_mean);
    if (variance < 0) {
        variance = 0;
    }

    const double res = std::sqrt(variance);
    return res;
}
