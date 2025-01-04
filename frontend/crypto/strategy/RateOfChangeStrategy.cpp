#include "RateOfChangeStrategy.h"

#include "Logger.h"

RateOfChangeStrategyConfig::RateOfChangeStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("sma_interval_m")) {
        m_sma_interval = std::chrono::minutes(json.get()["sma_interval_m"].get<int>());
    }
    if (json.get().contains("trigger_interval_m")) {
        m_trigger_interval = std::chrono::minutes(json.get()["trigger_interval_m"].get<int>());
    }
    if (json.get().contains("roc_interval_m")) {
        m_roc_interval = std::chrono::minutes(json.get()["roc_interval_m"].get<int>());
    }
    if (json.get().contains("signal_threshold")) {
        m_signal_threshold = json.get()["signal_threshold"].get<double>();
    }
}

bool RateOfChangeStrategyConfig::is_valid() const
{
    if (m_signal_threshold <= 0.) {
        Logger::logf<LogLevel::Error>("Invalid signal threshold: ", m_signal_threshold);
        return false;
    }
    if (m_sma_interval <= std::chrono::minutes(0)) {
        Logger::logf<LogLevel::Error>("Invalid trend interval: ", m_sma_interval);
        return false;
    }
    if (m_trigger_interval <= std::chrono::minutes(0)) {
        Logger::logf<LogLevel::Error>("Invalid trigger interval: ", m_trigger_interval);
        return false;
    }
    if (m_roc_interval <= std::chrono::minutes(0)) {
        Logger::logf<LogLevel::Error>("Invalid compare interval: ", m_roc_interval);
        return false;
    }
    return true;
}

JsonStrategyConfig RateOfChangeStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["sma_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_sma_interval).count();
    json["trigger_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_trigger_interval).count();
    json["roc_interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_roc_interval).count();
    json["signal_threshold"] = m_signal_threshold;
    return json;
}

RateOfChangeStrategy::RateOfChangeStrategy(const RateOfChangeStrategyConfig & config)
    : m_config(config)
    , m_rate_of_change(config.m_roc_interval)
    , m_moving_average(config.m_sma_interval)
{
}

bool RateOfChangeStrategy::is_valid() const
{
    return m_config.is_valid();
}

EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & RateOfChangeStrategy::strategy_internal_data_channel()
{
    return m_strategy_internal_data_channel;
}

std::optional<Signal> RateOfChangeStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    UNWRAP_RET_EMPTY(trend_price, m_moving_average.push_value(ts_and_price));
    const auto ts = ts_and_price.first;
    UNWRAP_RET_EMPTY(rate_of_change, m_rate_of_change.push_value({ts, trend_price}))

    m_strategy_internal_data_channel.push(
            ts,
            {"prices",
             "trend",
             trend_price});
    m_strategy_internal_data_channel.push(
            ts,
            {"rate_of_change",
             "upper_threshold",
             m_config.m_signal_threshold});
    m_strategy_internal_data_channel.push(
            ts,
            {"rate_of_change",
             "lower_threshold",
             -m_config.m_signal_threshold});
    m_strategy_internal_data_channel.push(
            ts,
            {"rate_of_change",
             "rate_of_change",
             rate_of_change});

    if (rate_of_change < m_config.m_signal_threshold && rate_of_change > -m_config.m_signal_threshold) {
        m_last_below_trigger_ts = ts;
    }

    if (ts - m_last_below_trigger_ts < m_config.m_trigger_interval) {
        return std::nullopt;
    }

    if (rate_of_change > m_config.m_signal_threshold) {
        const auto signal = Signal{.side = Side::buy(), .timestamp = ts_and_price.first, .price = ts_and_price.second};
        return signal;
    }

    const auto signal = Signal{.side = Side::sell(), .timestamp = ts_and_price.first, .price = ts_and_price.second};
    return signal;
}
