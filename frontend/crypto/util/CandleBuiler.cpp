#include "CandleBuiler.h"

#include "ScopeExit.h"

CandleBuilder::CandleBuilder(std::chrono::milliseconds timeframe)
    : m_timeframe(timeframe)
{
}

std::vector<Candle> CandleBuilder::push_trade(double price, SignedVolume volume, std::chrono::milliseconds timestamp)
{
    const auto current_timeframe_iter = timestamp.count() / m_timeframe.count();
    const auto saved_timeframe_iter = m_start.count() / m_timeframe.count();
    const auto current_start_ts = current_timeframe_iter * m_timeframe.count();

    // filling the current candle
    if (current_timeframe_iter == saved_timeframe_iter) {
        m_high = std::max(m_high, price);
        m_low = std::min(m_low, price);
        m_close = price;
        m_volume += volume.as_unsigned_and_side().first.value();
        return {};
    }

    ScopeExit se([&]() {
        m_start = std::chrono::milliseconds{current_start_ts};
    });

    std::vector<Candle> res;

    // it's not the very first trade
    if (m_start != std::chrono::milliseconds{}) {
        // pushing empty candles with close price
        for (int i = 0; i < current_timeframe_iter - saved_timeframe_iter - 1; ++i) {
            res.emplace_back(
                    m_timeframe,
                    (current_timeframe_iter + 1) * m_timeframe,
                    m_close,
                    m_close,
                    m_close,
                    m_close,
                    0.);
        }
        res.emplace_back(
                m_timeframe,
                m_start,
                m_open,
                m_high,
                m_low,
                m_close,
                m_volume);
    }

    // initializing the new candle
    m_open = price;
    m_high = price;
    m_low = price;
    m_close = price;
    m_volume = volume.as_unsigned_and_side().first.value();
    return res;
}
