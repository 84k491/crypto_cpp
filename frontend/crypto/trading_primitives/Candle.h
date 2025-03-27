#pragma once

#include "Side.h"
#include "nlohmann/json.hpp"

class Candle
{
public:
    Candle(
            std::chrono::milliseconds timeframe,
            std::chrono::milliseconds start_ts,
            double open,
            double high,
            double low,
            double close,
            double volume)
        : m_timeframe(timeframe)
        , m_timestamp(start_ts)
        , m_open(open)
        , m_high(high)
        , m_low(low)
        , m_close(close)
        , m_volume(volume)
    {
    }

    std::chrono::milliseconds timeframe() const { return m_timeframe; }
    std::chrono::milliseconds ts() const { return m_timestamp; }
    std::chrono::milliseconds close_ts() const { return m_timestamp + m_timeframe; }
    double open() const { return m_open; }
    double high() const { return m_high; }
    double low() const { return m_low; }
    double close() const { return m_close; }
    double volume() const { return m_volume; }
    Side side() const { return m_close > m_open ? Side::buy() : Side::sell(); }
    double price_diff() const { return m_close - m_open; }

    nlohmann::json to_json() const;

private:
    std::chrono::milliseconds m_timeframe;

    std::chrono::milliseconds m_timestamp;

    double m_open;
    double m_high;
    double m_low;
    double m_close;

    double m_volume;
};
