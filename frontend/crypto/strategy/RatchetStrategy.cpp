#include "RatchetStrategy.h"

RatchetStrategyConfig::RatchetStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("retracement")) {
        m_retracement = json.get()["retracement"].get<double>();
    }
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
}

bool RatchetStrategyConfig::is_valid() const
{
    return m_retracement > 0.;
}

JsonStrategyConfig RatchetStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["retracement"] = m_retracement;
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    return json;
}

RatchetStrategy::RatchetStrategy(RatchetStrategyConfig config)
    : m_config(config)
    , m_ratchet(config.m_retracement)
{
}

bool RatchetStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> RatchetStrategy::timeframe() const
{
    return m_config.m_timeframe;
}

std::optional<Signal> RatchetStrategy::push_candle(const Candle & c)
{
    const auto [value, flip] = m_ratchet.push_price_for_value(c.close());
    m_strategy_internal_data_channel.push(c.close_ts(), {"prices", "ratchet", value});

    if (!flip) {
        return {};
    }

    const auto side = value < c.close() ? Side::buy() : Side::sell();
    return Signal{.side = side, .timestamp = c.close_ts(), .price = c.close()};
}

EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & RatchetStrategy::strategy_internal_data_channel()
{
    return m_strategy_internal_data_channel;
}
