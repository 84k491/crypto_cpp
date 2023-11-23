#include "DoubleSmaStrategy.h"

DoubleSmaStrategy::DoubleSmaStrategy(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval)
    : m_slow_interval(slow_interval)
    , m_fast_interval(fast_interval)
{
}

namespace {
double get_average(const std::list<std::pair<uint64_t, double>> & data)
{
    double sum = 0;
    for (const auto & [ts, price] : data) {
        sum += price;
    }
    return sum / static_cast<double>(data.size());
}
} // namespace

std::optional<Signal> DoubleSmaStrategy::push_price(std::pair<uint64_t, double> ts_and_price)
{
    m_slow_data.push_back(ts_and_price);
    m_fast_data.push_back(ts_and_price);
    if ((m_slow_data.back().first - m_slow_data.front().first) < m_slow_interval.count()) {
        return std::nullopt;
    }
    else {
        m_slow_data.pop_front();
    }
    if ((m_fast_data.back().first - m_fast_data.front().first) < m_fast_interval.count()) {
        return std::nullopt;
    }
    else {
        m_fast_data.pop_front();
    }

    const auto current_average_slow = get_average(m_slow_data);
    const auto current_average_fast = get_average(m_fast_data);

    ScopeExit scope_exit([&] {
        m_prev_slow_avg = current_average_slow;
        m_prev_fast_avg = current_average_fast;
    });

    if (!m_prev_slow_avg.has_value() || !m_prev_fast_avg.has_value()) {
        return std::nullopt;
    }

    const auto current_slow_above_fast = current_average_slow > current_average_fast;
    const auto prev_slow_above_fast = m_prev_slow_avg.value() > m_prev_fast_avg.value();
    if (current_slow_above_fast == prev_slow_above_fast) {
        return std::nullopt;
    }

    const auto side = current_slow_above_fast ? Side::Sell : Side::Buy;
    return Signal{.side = side, .price = ts_and_price.second};
}
