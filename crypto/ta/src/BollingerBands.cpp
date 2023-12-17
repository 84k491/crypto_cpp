#include "BollingerBandsStrategy.h"

BollingerBandsStrategy::BollingerBandsStrategy(std::chrono::milliseconds interval, double std_deviation_coefficient)
    : m_trend(interval)
    , m_standard_deviation(interval)
    , m_interval(interval)
{
}

void BollingerBandsStrategy::subscribe_for_strategy_internal(std::function<void(std::string name,
                                                        std::chrono::milliseconds ts,
                                                        double data)> && cb)
{
    m_strategy_internal_callbacks.emplace_back(std::move(cb));
}

std::optional<Signal> BollingerBandsStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto & [ts, price] = ts_and_price;
    const auto standard_deviation = m_standard_deviation.push_value(ts_and_price);
    const auto trend = m_trend.push_value(ts_and_price);

    if (!standard_deviation.has_value() || !trend.has_value()) {
        return std::nullopt;
    }
    const auto current_standard_deviation = standard_deviation.value();
    const auto current_trend = trend.value();

    const auto upper_bb = current_trend + current_standard_deviation * m_std_deviation_coefficient;
    const auto lower_bb = current_trend - current_standard_deviation * m_std_deviation_coefficient;
    for (const auto & cb : m_strategy_internal_callbacks) {
        cb("upper_bollinger_band", ts, upper_bb);
        cb("trend", ts, current_trend);
        cb("lower_bollinger_band", ts, lower_bb);
    }

    if (price < lower_bb) {
        return Signal{Side::Buy, ts, upper_bb};
    }
    if (price > upper_bb) {
        return Signal{Side::Sell, ts, lower_bb};
    }
    return std::nullopt;
}
