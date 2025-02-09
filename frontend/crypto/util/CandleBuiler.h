#pragma once

#include "Candle.h"
#include "Volume.h"

class CandleBuilder
{
public:
    CandleBuilder(std::chrono::milliseconds timeframe);

    // builds candle only on a first trade of the next candle
    // so, there can be some empty candles if there are skipped candles in between
    // timestamps must be sorted in not-descending (>=) order
    std::vector<Candle> push_trade(double price, SignedVolume volume, std::chrono::milliseconds timestamp);

private:
    const std::chrono::milliseconds m_timeframe = {};

    std::chrono::milliseconds m_start = {};

    double m_open = 0;
    double m_high = 0;
    double m_low = 0;
    double m_close = 0;
    double m_volume = 0;
};
