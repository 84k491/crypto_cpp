#include "TimeWeightedMovingAverage.h"

#include "ScopeExit.h"

TimeWeightedMovingAverage::TimeWeightedMovingAverage(std::chrono::milliseconds interval)
    : m_interval(interval)
{
}

double TimeWeightedMovingAverage::calc_weight(size_t current)
{
    if (m_data.empty()) {
        return 0;
    }
    const auto prev = m_data.back().timestamp.count();
    return double(current - prev) / double(m_interval.count());
}

std::optional<double> TimeWeightedMovingAverage::push_value(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto & [timestamp, value] = ts_and_price;

    ScopeExit se{[&] {
        // we don't count current point, because we use time delta as "now - last",
        // and in this case it has 0 weight
        m_data.push_back(Data{.timestamp = timestamp, .value = value, .weight = 0.});
    }};

    if (m_data.empty()) {
        return {};
    }
    if (timestamp.count() < m_data.back().timestamp.count()) {
        return {};
    }

    Data & prev_point = m_data.back();
    double weight = calc_weight(timestamp.count());
    prev_point.weight = weight;

    m_sum += prev_point.value * prev_point.weight;
    m_total_weight += prev_point.weight;

    if (m_data.front().timestamp + m_interval > timestamp) {
        return {};
    }

    const auto res = m_sum / m_total_weight;

    m_sum -= m_data.front().value * m_data.front().weight;
    m_total_weight -= m_data.front().weight;
    m_data.pop_front();

    return res;
}
