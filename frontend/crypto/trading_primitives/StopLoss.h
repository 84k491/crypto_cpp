#pragma once

#include "Symbol.h"

#include <utility>

class StopLoss
{
public:
    StopLoss(double stop_price, UnsignedVolume vol, Symbol symbol)
        : m_stop_price(stop_price)
        , m_vol(std::move(vol))
        , m_symbol(std::move(symbol))
    {
    }

    double m_stop_price;
    UnsignedVolume m_vol;
    Symbol m_symbol;
};
