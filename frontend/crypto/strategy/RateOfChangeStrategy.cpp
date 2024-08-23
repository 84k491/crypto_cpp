#include "RateOfChangeStrategy.h"

#include "Logger.h"

RateOfChangeStrategyConfig::RateOfChangeStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("interval_m")) {
        m_interval = std::chrono::minutes(json.get()["interval_m"].get<int>());
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
    if (m_interval <= std::chrono::minutes(0)) {
        Logger::logf<LogLevel::Error>("Invalid interval: ", m_interval);
        return false;
    }
    return true;
}

JsonStrategyConfig RateOfChangeStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_interval).count();
    json["signal_threshold"] = m_signal_threshold;
    return json;
}

RateOfChangeStrategy::RateOfChangeStrategy(const RateOfChangeStrategyConfig & config)
    : m_config(config)
    , m_rate_of_change(config.m_interval)
{
}

bool RateOfChangeStrategy::is_valid() const
{
    return m_config.is_valid();
}

EventTimeseriesPublisher<std::pair<std::string, double>> & RateOfChangeStrategy::strategy_internal_data_publisher()
{
    return m_strategy_internal_data_publisher;
}

std::optional<Signal> RateOfChangeStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto rate_of_change_opt = m_rate_of_change.push_value(ts_and_price);
    if (!rate_of_change_opt.has_value()) {
        return std::nullopt;
    }
    const auto & rate_of_change = rate_of_change_opt.value();
    if (rate_of_change > m_config.m_signal_threshold) {
        const auto signal = Signal{.side = Side::Buy, .timestamp = ts_and_price.first, .price = ts_and_price.second};
        m_strategy_internal_data_publisher.push(ts_and_price.first, {"rate_of_change", rate_of_change});
        return signal;
    }
    if (rate_of_change < -m_config.m_signal_threshold) {
        const auto signal = Signal{.side = Side::Sell, .timestamp = ts_and_price.first, .price = ts_and_price.second};
        m_strategy_internal_data_publisher.push(ts_and_price.first, {"rate_of_change", rate_of_change});
        return signal;
    }
    return std::nullopt;
}
