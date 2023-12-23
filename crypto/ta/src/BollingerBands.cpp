#include "BollingerBands.h"

BollingerBands::BollingerBands(std::chrono::milliseconds interval, double std_deviation_coefficient)
    : m_interval(interval)
    , m_std_deviation_coefficient(std_deviation_coefficient)
    , m_standard_deviation(interval)
    , m_trend(interval)
{
}

std::optional<BollingerBandsValue> BollingerBands::push_value(std::pair<std::chrono::milliseconds, double> ts_and_price)
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

    return BollingerBandsValue{upper_bb, current_trend, lower_bb};
}
