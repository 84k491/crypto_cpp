#include "RatchetStrategy.h"

#include "EventLoopSubscriber.h"

RatchetStrategyConfig::RatchetStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("retracement")) {
        m_retracement = json.get()["retracement"].get<double>();
    }
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
    if (json.get().contains("risk")) {
        m_risk = json.get()["risk"].get<double>();
    }
    if (json.get().contains("no_loss_coef")) {
        m_no_loss_coef = json.get()["no_loss_coef"].get<double>();
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
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

JsonStrategyConfig RatchetStrategyConfig::make_exit_strategy_config() const
{
    nlohmann::json json;
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

RatchetStrategy::RatchetStrategy(
        RatchetStrategyConfig config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(config)
    , m_exit_strategy(orders,
                      config.make_exit_strategy_config(),
                      event_loop,
                      channels)
    , m_ratchet(config.m_retracement)
{
    m_channel_subs.push_back(channels.candle_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto &, const Candle & candle) {
                push_candle(candle);
            }));
}

bool RatchetStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> RatchetStrategy::timeframe() const
{
    return m_config.m_timeframe;
}

void RatchetStrategy::push_candle(const Candle & c)
{
    const auto [value, flip] = m_ratchet.push_price_for_value(c.close());
    m_strategy_internal_data_channel.push(c.close_ts(), {"prices", "ratchet", value});

    if (!flip) {
        return;
    }

    const auto side = value < c.close() ? Side::buy() : Side::sell();
    try_send_order(side, c.close(), c.close_ts());
}
