#include "NormalizedIndicator.h"

#include "Logger.h"

NormalizedIndicator::NormalizedIndicator(std::string chart_name, std::string series_name, bool inverted)
    : m_chart_name(std::move(chart_name))
    , m_series_name(std::move(series_name))
    , m_inverted(inverted)
{
}

StrategyInternalData NormalizedIndicator::push(double upper_threshold, double value, double lower_threshold) const
{
    if (upper_threshold < lower_threshold) {
        LOG_ERROR(
                "Upper threshold must be greater than lower threshold. chart_name: {}, series_name: {}",
                m_chart_name,
                m_series_name);
        return {.chart_name = m_chart_name, .series_name = m_series_name, .value = 0.0};
    }

    const auto half_interval = (upper_threshold - lower_threshold) / 2;
    const auto mid_point = lower_threshold + half_interval;

    auto value_normalized = (value - mid_point) / half_interval;

    if (value_normalized > 2.0) {
        value_normalized = 2.0;
    }
    else if (value_normalized < -2.0) {
        value_normalized = -2.0;
    }

    if (m_inverted) {
        value_normalized *= -1;
    }

    return {.chart_name = m_chart_name, .series_name = m_series_name, .value = value_normalized};
}
