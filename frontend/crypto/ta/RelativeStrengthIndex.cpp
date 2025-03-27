#include "RelativeStrengthIndex.h"

RelativeStrengthIndex::RelativeStrengthIndex(unsigned candles_interval)
    : m_candles_interval(candles_interval)
    , m_positive(m_candles_interval)
    , m_negative(m_candles_interval)
{
}

std::optional<double> RelativeStrengthIndex::push_candle(Candle c)
{
    const auto [pos_avg, neg_avg] = [&]() {
        const auto diff = c.price_diff();

        if (diff > 0) {
            return std::pair{m_positive.push(diff),
                             m_negative.push(0.)};
        }
        else {
            return std::pair{m_positive.push(0.),
            m_negative.push(-diff)};
        }
    }();

    if (!pos_avg.has_value() || !neg_avg.has_value()) {
        return std::nullopt;
    }

    const double rsi = 100 - (100 / (1 + *pos_avg / *neg_avg));
    return rsi;
}

RelativeStrengthIndex::Average::Average(unsigned interval)
    : m_interval(interval)
{
}

std::optional<double> RelativeStrengthIndex::Average::push(double value)
{
    m_data.push(value);
    m_sum += value;

    if (m_data.size() < m_interval) {
        return std::nullopt;
    }

    while (m_data.size() > m_interval) {
        m_sum -= m_data.front();
        m_data.pop();
    }

    return m_sum / static_cast<double>(m_data.size());
}
