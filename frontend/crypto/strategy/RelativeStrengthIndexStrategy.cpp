#include "RelativeStrengthIndexStrategy.h"

#include "EventLoopSubscriber.h"
#include "StrategyBase.h"

RelativeStrengthIndexStrategyConfig::RelativeStrengthIndexStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("margin")) {
        m_margin = json.get()["margin"].get<unsigned>();
    }
    if (json.get().contains("interval")) {
        m_interval = json.get()["interval"].get<unsigned>();
    }
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
}

JsonStrategyConfig RelativeStrengthIndexStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["margin"] = m_margin;
    json["interval"] = m_interval;
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    return json;
}

RelativeStrengthIndexStrategy::RelativeStrengthIndexStrategy(
        const RelativeStrengthIndexStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(config)
    , m_rsi(config.m_interval)
{
    m_channel_subs.push_back(channels.candle_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto &, const Candle & candle) {
                push_candle(candle);
            }));
}

void RelativeStrengthIndexStrategy::push_candle(const Candle & c)
{
    const auto rsi = m_rsi.push_candle(c);
    if (!rsi.has_value()) {
        return;
    }

    const double upper_trigger = 100 - m_config.m_margin;
    const double lower_trigger = m_config.m_margin;

    m_strategy_internal_data_channel.push(c.close_ts(), {"rsi", "upper", upper_trigger});
    m_strategy_internal_data_channel.push(c.close_ts(), {"rsi", "rsi", rsi.value()});
    m_strategy_internal_data_channel.push(c.close_ts(), {"rsi", "lower", lower_trigger});

    if (upper_trigger > rsi && rsi > lower_trigger) {
        return;
    }

    const auto side = rsi.value() > upper_trigger ? Side::buy() : Side::sell();

    try_send_order(side, c.close(), c.close_ts());
}

bool RelativeStrengthIndexStrategyConfig::is_valid() const
{
    return m_interval > 0 && m_margin > 0;
}

bool RelativeStrengthIndexStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> RelativeStrengthIndexStrategy::timeframe() const
{
    return m_config.m_timeframe;
}
