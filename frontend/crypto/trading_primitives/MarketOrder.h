#pragma once

#include "Volume.h"

#include <ostream>

class MarketOrder
{
    friend std::ostream & operator<<(std::ostream & os, const MarketOrder & order);

public:
    MarketOrder(std::string symbol, double price, UnsignedVolume volume, Side side)
        : m_symbol(std::move(symbol))
        , m_volume(std::move(volume))
        , m_side(side)
        , m_price(price)
    {
    }

    MarketOrder(std::string symbol, double price, SignedVolume signed_volume)
        : m_symbol(std::move(symbol))
        , m_volume(signed_volume.as_unsigned_and_side().first)
        , m_side(signed_volume.as_unsigned_and_side().second)
        , m_price(price)
    {
    }

    std::string side_str() const
    {
        if (m_side == Side::Buy) {
            return "Buy";
        }
        return "Sell";
    }

    auto volume() const { return m_volume; }
    auto symbol() const { return m_symbol; }
    auto side() const { return m_side; }
    auto price() const { return m_price; }

private:
    std::string m_symbol;
    UnsignedVolume m_volume;
    Side m_side = Side::Buy;
    double m_price = 0.;
};
std::ostream & operator<<(std::ostream & os, const MarketOrder & order);
