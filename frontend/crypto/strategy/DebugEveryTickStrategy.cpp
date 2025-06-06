#include "DebugEveryTickStrategy.h"

#include "EventLoopSubscriber.h"
#include "StrategyBase.h"

DebugEveryTickStrategyConfig::DebugEveryTickStrategyConfig(const JsonStrategyConfig &)
{
}

DebugEveryTickStrategyConfig::DebugEveryTickStrategyConfig(std::chrono::milliseconds, std::chrono::milliseconds)
{
}

bool DebugEveryTickStrategyConfig::is_valid()
{
    return true;
}

JsonStrategyConfig DebugEveryTickStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["slow_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_slow_interval).count();
    json["fast_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_fast_interval).count();
    return json;
}

DebugEveryTickStrategy::DebugEveryTickStrategy(
        const DebugEveryTickStrategyConfig & conf,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders)
    , m_config(conf)
{
    m_channel_subs.push_back(channels.price_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                push_price({ts, price});
            }));
}

void DebugEveryTickStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    iteration = (iteration + 1) % max_iter;
    if (iteration != 0) {
        return;
    }

    const auto side = last_side.opposite();
    last_side = side;

    try_send_order(side, ts_and_price.second, ts_and_price.first, {});
}

bool DebugEveryTickStrategy::is_valid() const
{
    return DebugEveryTickStrategyConfig::is_valid();
}
