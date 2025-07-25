#include "DoubleSmaStrategy.h"

#include "EventLoopSubscriber.h"
#include "ScopeExit.h"
#include "StrategyBase.h"

DoubleSmaStrategyConfig::DoubleSmaStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("slow_interval_m")) {
        m_slow_interval = std::chrono::minutes{json.get().at("slow_interval_m").get<int>()};
    }
    if (json.get().contains("fast_interval_m")) {
        m_fast_interval = std::chrono::minutes{json.get().at("fast_interval_m").get<int>()};
    }
}

bool DoubleSmaStrategyConfig::is_valid() const
{
    return m_slow_interval > m_fast_interval;
}

JsonStrategyConfig DoubleSmaStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["slow_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_slow_interval).count();
    json["fast_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_fast_interval).count();
    return json;
}

DoubleSmaStrategy::DoubleSmaStrategy(
        const DoubleSmaStrategyConfig & conf,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(conf)
    , m_slow_avg(conf.m_slow_interval)
    , m_fast_avg(conf.m_fast_interval)
{
    m_channel_subs.push_back(channels.price_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                push_price({ts, price});
            }));
}

void DoubleSmaStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto fast_avg = m_fast_avg.push_value(ts_and_price);
    const auto slow_avg = m_slow_avg.push_value(ts_and_price);

    if (!fast_avg.has_value()) {
        return;
    }
    const auto current_average_fast = fast_avg.value();
    m_strategy_internal_data_channel.push(ts_and_price.first, {"prices", "fast_avg_history", current_average_fast});

    if (!slow_avg.has_value()) {
        return;
    }
    const auto current_average_slow = slow_avg.value();
    m_strategy_internal_data_channel.push(ts_and_price.first, {"prices", "slow_avg_history", current_average_slow});

    ScopeExit scope_exit([&] {
        m_prev_slow_avg = current_average_slow;
        m_prev_fast_avg = current_average_fast;
    });

    if (!m_prev_slow_avg.has_value() || !m_prev_fast_avg.has_value()) {
        return;
    }

    const auto current_slow_above_fast = current_average_slow > current_average_fast;
    const auto prev_slow_above_fast = m_prev_slow_avg.value() > m_prev_fast_avg.value();
    if (current_slow_above_fast == prev_slow_above_fast) {
        return;
    }

    const auto side = current_slow_above_fast ? Side::sell() : Side::buy();
    try_send_order(side, ts_and_price.second, ts_and_price.first);
}

bool DoubleSmaStrategy::is_valid() const
{
    return m_config.is_valid();
}
