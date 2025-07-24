#include "DSMADiffStrategy.h"

#include "EventLoopSubscriber.h"
#include "StrategyBase.h"

DSMADiffStrategyConfig::DSMADiffStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("slow_interval_m")) {
        m_slow_interval = std::chrono::minutes{json.get().at("slow_interval_m").get<int>()};
    }
    if (json.get().contains("fast_interval_m")) {
        m_fast_interval = std::chrono::minutes{json.get().at("fast_interval_m").get<int>()};
    }
    if (json.get().contains("diff_threshold_percent")) {
        m_diff_threshold_percent = json.get().at("diff_threshold_percent").get<double>();
    }
}

bool DSMADiffStrategyConfig::is_valid() const
{
    return m_slow_interval > m_fast_interval && m_diff_threshold_percent > 0.;
}

JsonStrategyConfig DSMADiffStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["slow_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_slow_interval).count();
    json["fast_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_fast_interval).count();
    json["diff_threshold_percent"] = m_diff_threshold_percent;
    return json;
}

DSMADiffStrategy::DSMADiffStrategy(
        const DSMADiffStrategyConfig & conf,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(conf)
    , m_slow_avg(conf.m_slow_interval)
    , m_fast_avg(conf.m_fast_interval)
    , m_diff_threshold(conf.m_diff_threshold_percent / 100.)
{
    m_channel_subs.push_back(channels.price_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                push_price({ts, price});
            }));
}

void DSMADiffStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
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

    const double diff = current_average_fast - current_average_slow;
    const double diff_coef = diff / current_average_slow;
    const double diff_percent = diff_coef * 100.;
    m_strategy_internal_data_channel.push(ts_and_price.first, {"dsma_diff", "diff", diff_percent});

    if (diff_coef > m_diff_threshold) {
        try_send_order(Side::buy(), ts_and_price.second, ts_and_price.first);
    }
    else if (diff_coef < -m_diff_threshold) {
        try_send_order(Side::sell(), ts_and_price.second, ts_and_price.first);
    }
}

bool DSMADiffStrategy::is_valid() const
{
    return m_config.is_valid();
}
