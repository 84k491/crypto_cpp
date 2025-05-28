#include "DebugEveryTickStrategy.h"

#include "EventLoopSubscriber.h"

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
        EventTimeseriesChannel<double> & price_channel)
    : m_config(conf)
{
    m_channel_subs.push_back(price_channel.subscribe(
            event_loop.m_event_loop,
            [](auto) {},
            [this](const auto & ts, const double & price) {
                if (const auto signal_opt = push_price({ts, price}); signal_opt) {
                    m_signal_channel.push(ts, signal_opt.value());
                }
            }));
}

std::optional<Signal> DebugEveryTickStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    iteration = (iteration + 1) % max_iter;
    if (iteration != 0) {
        return std::nullopt;
    }

    const auto side = last_side.opposite();
    last_side = side;

    return Signal{.side = side, .timestamp = ts_and_price.first, .price = ts_and_price.second};
}

bool DebugEveryTickStrategy::is_valid() const
{
    return DebugEveryTickStrategyConfig::is_valid();
}
