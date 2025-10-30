#pragma once

#include "Candle.h"
#include "Volume.h"

/*
    Builds candle only on a first trade of the next candle
    so, there can be some empty candles if there were no trades in between.

    Uses guarantee: timestamps must be sorted in not-descending (>=) order

    Provides guarantee: output candle timestamps are always increasing in the vector
*/
class CandleBuilder
{
public:
    CandleBuilder(std::chrono::milliseconds timeframe);

    std::vector<Candle> push_trade(double price, SignedVolume volume, std::chrono::milliseconds timestamp); // TODO use PublicTrade as arg

private:
    const std::chrono::milliseconds m_timeframe = {};

    std::chrono::milliseconds m_start = {};

    double m_open = 0.;
    double m_high = 0.;
    double m_low = 0.;
    double m_close = 0.;
    double m_buy_taker_volume = 0.;
    double m_sell_taker_volume = 0.;
    size_t m_trades_count = 0;
};
