#include "DoubleSmaStrategy.h"

#include "ScopeExit.h"

#include <iostream>

DoubleSmaStrategyConfig::DoubleSmaStrategyConfig(std::chrono::milliseconds slow_interval, std::chrono::milliseconds fast_interval)
    : m_slow_interval(slow_interval)
    , m_fast_interval(fast_interval)
{
}

bool DoubleSmaStrategyConfig::is_valid() const
{
    return m_slow_interval > m_fast_interval;
}

MovingAverage::MovingAverage(std::chrono::milliseconds interval)
    : m_interval(interval)
{
}

std::optional<double> MovingAverage::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    m_data.push_back(ts_and_price);
    m_sum += static_cast<size_t>(ts_and_price.second);

    if ((m_data.back().first - m_data.front().first) < m_interval) {
        return std::nullopt;
    }

    m_sum -= static_cast<size_t>(m_data.front().second);
    m_data.pop_front();

    return static_cast<double>(m_sum) / static_cast<double>(m_data.size());
}

DoubleSmaStrategy::DoubleSmaStrategy(const DoubleSmaStrategyConfig & conf)
    : m_config(conf)
    , m_slow_avg(conf.m_slow_interval)
    , m_fast_avg(conf.m_fast_interval)
{
}

std::optional<Signal> DoubleSmaStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto fast_avg = m_fast_avg.push_price(ts_and_price);
    const auto slow_avg = m_slow_avg.push_price(ts_and_price);

    if (!fast_avg.has_value()) {
        return std::nullopt;
    }
    const auto current_average_fast = fast_avg.value();
    m_fast_avg_history.emplace_back(ts_and_price.first, current_average_fast);
    for (const auto & cb : m_strategy_internal_callbacks) {
        cb("fast_avg_history", ts_and_price.first, current_average_fast);
    }

    if (!slow_avg.has_value()) {
        return std::nullopt;
    }
    const auto current_average_slow = slow_avg.value();
    m_slow_avg_history.emplace_back(ts_and_price.first, current_average_slow);
    for (const auto & cb : m_strategy_internal_callbacks) {
        cb("slow_avg_history", ts_and_price.first, current_average_slow);
    }

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
    return Signal{.side = side, .timestamp = ts_and_price.first, .price = ts_and_price.second};
}

std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>>
DoubleSmaStrategy::get_internal_data_history() const
{
    return {
            {"slow_avg_history", m_slow_avg_history},
            {"fast_avg_history", m_fast_avg_history},
    };
}

void DoubleSmaStrategy::subscribe_for_strategy_internal(std::function<void(std::string name,
                                                                           std::chrono::milliseconds ts,
                                                                           double data)> && cb)
{
    m_strategy_internal_callbacks.emplace_back(std::move(cb));
}
