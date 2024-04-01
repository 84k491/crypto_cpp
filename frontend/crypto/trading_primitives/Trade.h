#pragma once

#include "Volume.h"

#include <chrono>

class Trade
{
    friend std::ostream & operator<<(std::ostream & os, const Trade & trade);

public:
    Trade(std::chrono::milliseconds ts, std::string symbol, double price, UnsignedVolume volume, Side side, double fee)
        : m_ts(ts)
        , m_symbol(std::move(symbol))
        , m_price(price)
        , m_unsigned_volume(std::move(volume))
        , m_side(side)
        , m_fee(fee)
    {
    }

    std::chrono::milliseconds ts() const { return m_ts; }
    std::string symbol() const { return m_symbol; }
    double price() const { return m_price; }
    UnsignedVolume unsigned_volume() const { return m_unsigned_volume; }
    Side side() const { return m_side; }
    double fee() const { return m_fee; }

private:
    std::chrono::milliseconds m_ts;
    std::string m_symbol;
    double m_price{};
    UnsignedVolume m_unsigned_volume;
    Side m_side = Side::Buy;
    double m_fee = 0.0;
};
std::ostream & operator<<(std::ostream & os, const Trade & trade);
