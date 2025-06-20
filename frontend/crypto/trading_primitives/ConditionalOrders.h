#pragma once

#include "MarketOrder.h"
#include "Volume.h"
#include "crossguid/guid.hpp"

#include <chrono>
#include <string>

class ConditionalMarketOrder
{
public:
    enum class Type
    {
        StopLoss,
        TakeProfit,
    };

    ConditionalMarketOrder(
            std::string symbol,
            double price,
            UnsignedVolume volume,
            Side side,
            Type conditional_type,
            std::chrono::milliseconds signal_ts)
        : m_guid(xg::newGuid())
        , m_market_order(symbol, price, volume, side, signal_ts)
        , m_trigger_price(price)
        , m_type(conditional_type)
    {
    }

    auto guid() const { return m_market_order.guid(); }
    auto & order() const { return m_market_order; }
    auto & trigger_price() const { return m_trigger_price; }
    auto & type() const { return m_type; }

private:
    xg::Guid m_guid;
    MarketOrder m_market_order;
    double m_trigger_price = 0.;
    Type m_type;
};

class StopLossMarketOrder : public ConditionalMarketOrder
{
public:
    StopLossMarketOrder(
            std::string symbol,
            double price,
            UnsignedVolume volume,
            Side side,
            std::chrono::milliseconds signal_ts)
        : ConditionalMarketOrder(
                  symbol,
                  price,
                  volume,
                  side,
                  ConditionalMarketOrder::Type::StopLoss,
                  signal_ts)
    {
    }
};

class TakeProfitMarketOrder : public ConditionalMarketOrder
{
public:
    TakeProfitMarketOrder(
            std::string symbol,
            double price,
            UnsignedVolume volume,
            Side side,
            std::chrono::milliseconds signal_ts)
        : ConditionalMarketOrder(
                  symbol,
                  price,
                  volume,
                  side,
                  ConditionalMarketOrder::Type::TakeProfit,
                  signal_ts)
    {
    }
};
