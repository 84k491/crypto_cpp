#pragma once

#include "Volume.h"

#include <chrono>

class Trade
{
    friend std::ostream & operator<<(std::ostream & os, const Trade & trade);
public:
    Trade(std::chrono::milliseconds ts, std::string symbol, double price, UnsignedVolume volume, Side side, double fee)
        : ts(ts)
        , symbol(std::move(symbol))
        , price(price)
        , unsigned_volume(std::move(volume))
        , side(side)
        , fee(fee)
    {
    }

public: // TODO make private with getters
        // private:
    std::chrono::milliseconds ts;
    std::string symbol;
    double price{};
    UnsignedVolume unsigned_volume;
    Side side = Side::Buy;
    double fee = 0.0;
};
std::ostream & operator<<(std::ostream & os, const Trade & trade);
