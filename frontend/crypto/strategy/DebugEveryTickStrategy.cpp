#include "DebugEveryTickStrategy.h"

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

DebugEveryTickStrategy::DebugEveryTickStrategy(const DebugEveryTickStrategyConfig & conf)
    : m_config(conf)
{
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

std::optional<std::chrono::milliseconds> DebugEveryTickStrategy::timeframe() const
{
    return {};
}

EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & DebugEveryTickStrategy::strategy_internal_data_channel()
{
    return m_strategy_internal_data_channel;
}
