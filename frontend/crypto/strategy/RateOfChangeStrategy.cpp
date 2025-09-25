#include "RateOfChangeStrategy.h"

#include "Logger.h"
#include "StrategyBase.h"

RateOfChangeStrategyConfig::RateOfChangeStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
    if (json.get().contains("trigger_interval_m")) {
        m_trigger_interval = json.get()["trigger_interval_m"].get<int>();
    }
    if (json.get().contains("signal_threshold")) {
        m_signal_threshold = json.get()["signal_threshold"].get<double>();
    }
    if (json.get().contains("risk")) {
        m_risk = json.get()["risk"].get<double>();
    }
    if (json.get().contains("no_loss_coef")) {
        m_no_loss_coef = json.get()["no_loss_coef"].get<double>();
    }
}

bool RateOfChangeStrategyConfig::is_valid() const
{
    if (m_signal_threshold <= 0.) {
        Logger::logf<LogLevel::Error>("Invalid signal threshold: ", m_signal_threshold);
        return false;
    }
    if (m_trigger_interval <= 1) {
        Logger::logf<LogLevel::Error>("Invalid trigger interval: ", m_trigger_interval);
        return false;
    }
    return true;
}

JsonStrategyConfig RateOfChangeStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    json["trigger_interval_m"] = m_trigger_interval;
    json["signal_threshold"] = m_signal_threshold;
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

JsonStrategyConfig RateOfChangeStrategyConfig::make_exit_strategy_config() const
{
    nlohmann::json json;
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

RateOfChangeStrategy::RateOfChangeStrategy(
        const RateOfChangeStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(config)
    , m_exit_strategy(orders,
                      config.make_exit_strategy_config(),
                      event_loop,
                      channels)
{
    event_loop.subscribe(
            channels.candle_channel,
            [](const auto &) {},
            [this](const auto &, const Candle & candle) {
                push_candle(candle);
            });
}

bool RateOfChangeStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> RateOfChangeStrategy::timeframe() const
{
    return m_config.m_timeframe;
}

int sign(int v)
{
    if (v > 0) {
        return 1;
    }
    else if (v < 0) {
        return -1;
    }
    else {
        return 0;
    }
}

void RateOfChangeStrategy::push_candle(const Candle & c)
{
    const auto close_ts = c.close_ts();

    const double roc = (c.close() - m_prev_closing_prices.back()) / m_prev_closing_prices.back();
    {
        m_prev_closing_prices.push_front(c.close());
        while (m_prev_closing_prices.size() > s_roc_interval) {
            m_prev_closing_prices.pop_back();
        }
    }

    m_strategy_internal_data_channel.push(
            close_ts,
            {"rate_of_change",
             "upper_threshold",
             m_config.m_signal_threshold});
    m_strategy_internal_data_channel.push(
            close_ts,
            {"rate_of_change",
             "lower_threshold",
             -m_config.m_signal_threshold});
    m_strategy_internal_data_channel.push(
            close_ts,
            {"rate_of_change",
             "rate_of_change",
             roc});

    const bool in_trigger_zone = roc >= m_config.m_signal_threshold || roc <= -m_config.m_signal_threshold;
    const bool signs_match = m_trigger_iter == 0 || sign(m_trigger_iter) == c.side().sign();
    const bool triggered = in_trigger_zone && signs_match;
    if (triggered) {
        m_trigger_iter += c.side().sign();
    }
    else {
        m_trigger_iter = 0;
    }

    if (m_trigger_iter < m_config.m_trigger_interval) {
        return;
    }

    if (roc > m_config.m_signal_threshold) {
        try_send_order(Side::buy(), c.close(), close_ts);
        return;
    }

    try_send_order(Side::sell(), c.close(), close_ts);
}
