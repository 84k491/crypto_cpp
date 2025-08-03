#pragma once

#include "Side.h"
#include "crossguid/guid.hpp"

#include <chrono>
#include <optional>
#include <utility>

// TODO use StopLoss from ConditionalOrders.h
// used as TrailingStopLoss current value
class StopLoss
{
public:
    StopLoss(std::string symbol_name, double stop_price, Side side)
        : m_symbol_name(std::move(symbol_name))
        , m_stop_price(stop_price)
        , m_side(side)
    {
    }

    auto stop_price() const { return m_stop_price; }
    auto side() const { return m_side; }
    auto symbol_name() const { return m_symbol_name; }

private:
    std::string m_symbol_name;
    double m_stop_price;
    Side m_side;
};

class TrailingStopLoss
{
public:
    TrailingStopLoss(std::string symbol_name, double price_distance, Side side)
        : m_guid(xg::newGuid())
        , m_symbol_name(std::move(symbol_name))
        , m_price_distance(price_distance)
        , m_side(side)
    {
    }

    auto price_distance() const { return m_price_distance; }
    auto side() const { return m_side; } // opposite to position's
    auto symbol_name() const { return m_symbol_name; }
    auto guid() const { return m_guid; }

    // TODO make two methods. One with opt return, one with guaranteed return
    std::optional<StopLoss> calc_new_stop_loss(const double current_price, const std::optional<StopLoss> & previous_stop_loss) const;

    std::optional<double> m_active_stop_loss_price;
    std::chrono::milliseconds m_update_ts{};

private:
    xg::Guid m_guid;
    std::string m_symbol_name;
    double m_price_distance;
    Side m_side;
};
