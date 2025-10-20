#include "StaticGridStrategy.h"

StaticGridStrategyConfig::StaticGridStrategyConfig(JsonStrategyConfig json)
{
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
    if (json.get().contains("interval")) {
        m_interval = json.get()["interval"].get<int>();
    }
    if (json.get().contains("level_width_perc")) {
        m_level_width_perc = json.get()["level_width_perc"].get<decltype(m_level_width_perc)>();
    }
}

bool StaticGridStrategyConfig::is_valid() const
{
    return true;
}

JsonStrategyConfig StaticGridStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    json["interval"] = m_interval;
    json["level_width_perc"] = m_level_width_perc;
    return json;
}

StaticGridStrategy::StaticGridStrategy(
        const StaticGridStrategyConfig & config,
        EventLoop & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StaticGridWithBan(
              config.to_json(),
              event_loop,
              channels,
              orders)
{
}

bool StaticGridStrategy::is_banned(std::chrono::milliseconds, const Candle &)
{
    const bool later_than_start = m_current_interval_start_ts < m_last_out_of_bounds_price_ts;
    const bool before_end = m_last_out_of_bounds_price_ts < m_current_interval_start_ts + (m_config.m_interval * m_config.m_timeframe);
    return later_than_start && before_end;
}
