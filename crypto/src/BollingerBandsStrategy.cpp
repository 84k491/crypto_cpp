#include "BollingerBandsStrategy.h"

BollingerBandsStrategyConfig::BollingerBandsStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("std_dev")) {
        m_std_deviation_coefficient = json.get()["std_dev"].get<double>();
    }
    if (json.get().contains("interval_m")) {
        m_interval = std::chrono::minutes(json.get()["interval_m"].get<int>());
    }
}

bool BollingerBandsStrategyConfig::is_valid() const
{
    return m_interval > std::chrono::minutes(0) && m_std_deviation_coefficient > 0.;
}

JsonStrategyConfig BollingerBandsStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["std_dev"] = m_std_deviation_coefficient;
    json["interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_interval).count();
    return json;
}

BollingerBandsStrategy::BollingerBandsStrategy(const BollingerBandsStrategyConfig & config)
    : m_config(config)
    , m_bollinger_bands(config.m_interval, config.m_std_deviation_coefficient)
{
}

bool BollingerBandsStrategy::is_valid() const
{
    return m_config.is_valid();
}

void BollingerBandsStrategy::subscribe_for_strategy_internal(std::function<void(std::string name,
                                                                                std::chrono::milliseconds ts,
                                                                                double data)> && cb)
{
    m_strategy_internal_callbacks.emplace_back(std::move(cb));
}

std::optional<Signal> BollingerBandsStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto bb_res_opt = m_bollinger_bands.push_value(ts_and_price);
    if (!bb_res_opt.has_value()) {
        return std::nullopt;
    }
    const auto & bb_res = bb_res_opt.value();
    const auto & [ts, price] = ts_and_price;
    for (const auto & cb : m_strategy_internal_callbacks) {
        cb("upper_band", ts, bb_res.m_upper_band);
        cb("lower_band", ts, bb_res.m_lower_band);
        cb("trend", ts, bb_res.m_trend);
    }

    if (price > bb_res.m_upper_band) {
        return Signal{.side = Side::Sell, .timestamp = ts, .price = ts_and_price.second};
    }
    if (price < bb_res.m_lower_band) {
        return Signal{.side = Side::Buy, .timestamp = ts, .price = ts_and_price.second};
    }
    return std::nullopt;
}
