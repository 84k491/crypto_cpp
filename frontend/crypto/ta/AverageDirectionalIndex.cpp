#include "AverageDirectionalIndex.h"

#include "ScopeExit.h"

#include <algorithm>
#include <tuple>

AverageDirectionalIndex::AverageDirectionalIndex(size_t interval, std::chrono::milliseconds timeframe)
    : m_average_true_range(timeframe * interval)
    , m_smoothed_pdm(timeframe * interval)
    , m_smoothed_ndm(timeframe * interval)
    , m_average_directional_index(timeframe * interval)
{
}

std::optional<AverageDirectionalIndex::Result> AverageDirectionalIndex::push_candle(const Candle & current_candle)
{
    ScopeExit se{[&] { m_previous_candle = current_candle; }};

    const auto ts = current_candle.close_ts();

    const auto atr_opt = m_average_true_range.push_value({ts, true_range(current_candle)});
    const auto dm = directional_movement(current_candle);
    const auto spdm_opt = m_smoothed_pdm.push_value({ts, dm.positive});
    const auto sndm_opt = m_smoothed_ndm.push_value({ts, dm.negative});

    if (!atr_opt.has_value() || !spdm_opt.has_value() || !sndm_opt.has_value()) {
        return {};
    }
    const auto [atr, spdm, sndm] = std::tie(atr_opt.value(), spdm_opt.value(), sndm_opt.value());

    const double pdi = 100. * spdm / atr;
    const double ndi = 100. * sndm / atr;
    const double dx = 100. * std::fabs((pdi - ndi) / (pdi + ndi));

    const auto adx_opt = m_average_directional_index.push_value({ts, dx});
    if (!adx_opt.has_value()) {
        return {};
    }

    Direction direction = pdi > ndi ? Direction::UpTrend : Direction::DownTrend;

    return {{.adx = adx_opt.value(), .trend = direction}};
}

AverageDirectionalIndex::DirectionalMovement AverageDirectionalIndex::directional_movement(const Candle & current_candle)
{
    double ph = 0.;
    double pl = 0.;
    if (m_previous_candle.has_value()) {
        ph = m_previous_candle->high();
        pl = m_previous_candle->low();
    }

    double pos = std::max<double>(current_candle.high() - ph, 0);
    double neg = std::max<double>(pl - current_candle.low(), 0);

    if (pos > neg) {
        neg = 0.;
    }
    else {
        pos = 0.;
    }

    return {.positive = pos, .negative = neg};
}

double AverageDirectionalIndex::true_range(const Candle & current_candle)
{
    const double cur_high_cur_low = current_candle.high() - current_candle.low();

    if (!m_previous_candle.has_value()) {
        return cur_high_cur_low;
    }
    const Candle & prev_candle = m_previous_candle.value();

    const double cur_high_prev_close = std::fabs(current_candle.high() - prev_candle.close());
    const double cur_low_prev_close = std::fabs(current_candle.low() - prev_candle.close());

    return std::max({cur_high_cur_low, cur_high_prev_close, cur_low_prev_close});
}
