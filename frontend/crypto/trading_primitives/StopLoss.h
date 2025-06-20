#pragma once

#include "MarketOrder.h"
#include "Volume.h"

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
        : m_market_order(symbol, price, volume, side, signal_ts)
        , m_type(conditional_type)
    {
    }

private:
    MarketOrder m_market_order;
    Type m_type;
};

class StopLossMarketOrder : public ConditionalMarketOrder
{
};
