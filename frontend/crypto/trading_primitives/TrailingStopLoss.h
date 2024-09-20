#pragma once

#include "Side.h"
#include "Symbol.h"

#include <utility>

// used as TrailingStopLoss current value
class StopLoss
{
public:
    StopLoss(Symbol symbol, double stop_price, Side side)
        : m_symbol(std::move(symbol))
        , m_stop_price(stop_price)
        , m_side(side)
    {
    }

    auto stop_price() const { return m_stop_price; }
    auto side() const { return m_side; }
    auto symbol() const { return m_symbol; }

private:
    Symbol m_symbol;
    double m_stop_price;
    Side m_side;
};

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

    // TODO make two methods. One with opt return, one with guaranteed return
    std::optional<StopLoss> calc_new_stop_loss(const double current_price, const std::optional<StopLoss> & previous_stop_loss) const;

private:
    Symbol m_symbol;
    double m_price_distance;
    Side m_side;
};
