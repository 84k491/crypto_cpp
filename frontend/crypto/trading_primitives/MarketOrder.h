#pragma once

#include "Volume.h"

#include <ostream>

class MarketOrder
{
    friend std::ostream & operator<<(std::ostream & os, const MarketOrder & order);

public:
    MarketOrder(std::string symbol, UnsignedVolume volume, Side side)
        : m_symbol(std::move(symbol))
        , m_volume(std::move(volume))
        , m_side(side)
    {
    }

    MarketOrder(std::string symbol, SignedVolume signed_volume)
        : m_symbol(std::move(symbol))
        , m_volume(signed_volume.as_unsigned_and_side().first)
        , m_side(signed_volume.as_unsigned_and_side().second)
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

private:
    std::string m_symbol;
    UnsignedVolume m_volume;
    Side m_side = Side::Buy;
};
std::ostream & operator<<(std::ostream & os, const MarketOrder & order);
