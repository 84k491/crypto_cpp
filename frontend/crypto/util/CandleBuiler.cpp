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

    ScopeExit se([&]() {
        m_start = std::chrono::milliseconds{current_start_ts};
    });

    // filling the current candle
    if (current_timeframe_iter == saved_timeframe_iter) {
        m_high = std::max(m_high, price);
        m_low = std::min(m_low, price);
        m_close = price;

        const auto side = volume.as_unsigned_and_side().second;
        if (side == Side::buy()) {
            m_buy_taker_volume += volume.as_unsigned_and_side().first.value();
        }
        else {
            m_sell_taker_volume += volume.as_unsigned_and_side().first.value();
        }

        m_trades_count += 1;

        return {};
    }

    std::vector<Candle> res;

    // it's not the very first trade since creation
    if (m_start != std::chrono::milliseconds{}) {
        res.emplace_back(
                m_timeframe,
                m_start,
                m_open,
                m_high,
                m_low,
                m_close,
                m_buy_taker_volume,
                m_sell_taker_volume,
                m_trades_count);

        // pushing empty candles with close price
        for (int i = 1; i < current_timeframe_iter - saved_timeframe_iter; ++i) {
            res.emplace_back(
                    m_timeframe,
                    (saved_timeframe_iter + i) * m_timeframe,
                    m_close,
                    m_close,
                    m_close,
                    m_close,
                    0.,
                    0.,
                    0);
        }
    }

    // initializing the new candle
    m_open = price;
    m_high = price;
    m_low = price;
    m_close = price;

    const auto side = volume.as_unsigned_and_side().second;
    if (side == Side::buy()) {
        m_buy_taker_volume = volume.as_unsigned_and_side().first.value();
        m_sell_taker_volume = 0;
    }
    else {
        m_buy_taker_volume = 0;
        m_sell_taker_volume = volume.as_unsigned_and_side().first.value();
    }

    m_trades_count = 1;

    return res;
}
