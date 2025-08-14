#include "DynamicGridAdxStrategy.h"

DynamicGridAdxStrategyConfig::DynamicGridAdxStrategyConfig(JsonStrategyConfig json)
{
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
    if (json.get().contains("interval")) {
        m_interval = json.get()["interval"].get<int>();
    }
    if (json.get().contains("levels_per_side")) {
        m_levels_per_side = json.get()["levels_per_side"].get<decltype(m_levels_per_side)>();
    }
    if (json.get().contains("price_radius_perc")) {
        m_price_radius_perc = json.get()["price_radius_perc"].get<decltype(m_price_radius_perc)>();
    }
    if (json.get().contains("adx_interval")) {
        m_adx_interval = json.get()["adx_interval"].get<int>();
    }
    if (json.get().contains("adx_threshold")) {
        m_adx_threshold = json.get()["adx_threshold"].get<decltype(m_adx_threshold)>();
    }
}

bool DynamicGridAdxStrategyConfig::is_valid() const
{
    return m_levels_per_side > 0;
}

JsonStrategyConfig DynamicGridAdxStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    json["interval"] = m_interval;
    json["levels_per_side"] = m_levels_per_side;
    json["price_radius_perc"] = m_price_radius_perc;
    json["adx_interval"] = m_adx_interval;
    json["adx_threshold"] = m_adx_threshold;
    return json;
}

DynamicGridAdxStrategy::DynamicGridAdxStrategy(
        const DynamicGridAdxStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : DynamicGridWithBan(config.to_json(), event_loop, channels, orders)
    , m_config(config)
    , m_adx(config.m_adx_interval * config.m_timeframe)
{
}

bool DynamicGridAdxStrategy::is_valid() const
{
    return true;
}

std::optional<std::chrono::milliseconds> DynamicGridAdxStrategy::timeframe() const
{
    return m_config.m_timeframe;
}

bool DynamicGridAdxStrategy::is_banned(std::chrono::milliseconds ts, const Candle & candle)
{
    const auto adx_opt = m_adx.push_candle(candle);
    if (!adx_opt.has_value()) {
        return true;
    }
    const auto & adx = adx_opt.value();
    m_strategy_internal_data_channel.push(ts, {.chart_name = "adx", .series_name = "adx", .value = adx.adx});

    if (adx.adx > m_config.m_adx_threshold) {
        m_ban_until = ts + m_config.m_timeframe * m_config.m_interval; // for trend interval
        m_strategy_internal_data_channel.push(ts, {.chart_name = "adx", .series_name = "threshold", .value = m_config.m_adx_threshold});
    }

    return m_ban_until > ts;
}
