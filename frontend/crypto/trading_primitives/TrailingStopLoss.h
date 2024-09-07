#pragma once

#include "Enums.h"
#include "Symbol.h"

#include <utility>

class TrailingStopLoss
{
public:
    TrailingStopLoss(Symbol symbol, double price_distance, Side side)
        : m_symbol(std::move(symbol))
        , m_price_distance(price_distance)
        , m_side(side)
    {
    }

    auto price_distance() const { return m_price_distance; }
    auto side() const { return m_side; }
    auto symbol() const { return m_symbol; }

private:
    Symbol m_symbol;
    double m_price_distance;
    Side m_side;
};
