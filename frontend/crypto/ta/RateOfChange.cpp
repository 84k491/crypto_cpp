#include "RateOfChange.h"

RateOfChange::RateOfChange(std::chrono::milliseconds interval)
    : m_interval(interval)
{
}

std::optional<double> RateOfChange::push_value(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    m_data.push_back(ts_and_price);
    const auto ts_diff = [&]() -> std::chrono::milliseconds {
        const auto & [head_ts, _1] = m_data.front();
        const auto & [tail_ts, _2] = m_data.back();
        return tail_ts - head_ts;
    };
    if (ts_diff() < m_interval) {
        return std::nullopt;
    }
    const auto head_value = m_data.front().second;
    const auto tail_value = m_data.back().second;
    const auto res = 100 * (tail_value - head_value) / static_cast<double>(head_value);

    while (ts_diff() > m_interval) {
        m_data.pop_front();
    }
    return res;
}
