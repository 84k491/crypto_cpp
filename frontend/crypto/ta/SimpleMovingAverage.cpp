#include "SimpleMovingAverage.h"

SimpleMovingAverage::SimpleMovingAverage(std::chrono::milliseconds interval)
    : m_interval(interval)
{
}

std::optional<double> SimpleMovingAverage::push_value(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    m_data.push_back(ts_and_price);
    m_sum += ts_and_price.second;

    if ((m_data.back().first - m_data.front().first) < m_interval) {
        return std::nullopt;
    }

    m_sum -= m_data.front().second;
    m_data.pop_front();

    return m_sum / static_cast<double>(m_data.size());
}

