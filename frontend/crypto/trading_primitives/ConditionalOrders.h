#pragma once

#include "MarketOrder.h"
#include "Volume.h"
#include "crossguid/guid.hpp"

#include <chrono>
#include <string>

class ConditionalMarketOrder : public MarketOrder
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
        : MarketOrder(symbol, price, volume, side, signal_ts)
        , m_guid(xg::newGuid())
        , m_trigger_price(price)
        , m_type(conditional_type)
    {
    }

    auto & trigger_price() const { return m_trigger_price; }
    auto & type() const { return m_type; }
    OrderStatus status() const override;

private:
    xg::Guid m_guid;
    UnsignedVolume m_suspended_volume;
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
