#pragma once

#include "Side.h"
#include "Volume.h"

#include "nlohmann/json.hpp"

#include <chrono>

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
            double buy_taker_volume,
            double sell_taker_volume,
            size_t num_of_trades)
        : m_timeframe(timeframe)
        , m_timestamp(start_ts)
        , m_open(open)
        , m_high(high)
        , m_low(low)
        , m_close(close)
        , m_buy_taker_volume(buy_taker_volume)
        , m_sell_taker_volume(sell_taker_volume)
        , m_trade_count(num_of_trades)
    {
    }

    std::chrono::milliseconds timeframe() const { return m_timeframe; }
    std::chrono::milliseconds ts() const { return m_timestamp; }
    // pass-the-end
    std::chrono::milliseconds close_ts() const { return m_timestamp + m_timeframe; }

    double open() const { return m_open; }
    double high() const { return m_high; }
    double low() const { return m_low; }
    double close() const { return m_close; }
    Side side() const { return m_close > m_open ? Side::buy() : Side::sell(); }

    double volume() const { return m_buy_taker_volume + m_sell_taker_volume; }
    double buy_taker_volume() const { return m_buy_taker_volume; }
    double sell_taker_volume() const { return m_sell_taker_volume; }

    struct DisbalanceCoef
    {
        double coef;
        Side side;
    };
    DisbalanceCoef volume_disbalance_coef() const;
    SignedVolume volume_disbalance() const;

    double price_diff() const { return m_close - m_open; }
    double price_diff_coef() const { return (m_close - m_open) / m_open; }
    size_t trade_count() const { return m_trade_count; }
    double absorption() const { return m_trade_count / price_diff_coef(); }

    nlohmann::json to_json() const;

private:
    std::chrono::milliseconds m_timeframe;
    std::chrono::milliseconds m_timestamp;

    double m_open;
    double m_high;
    double m_low;
    double m_close;

    double m_buy_taker_volume;
    double m_sell_taker_volume;

    size_t m_trade_count;
};
