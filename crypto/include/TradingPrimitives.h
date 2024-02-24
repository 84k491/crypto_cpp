#pragma once

#include "Types.h"

#include <chrono>
#include <string>

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

class Trade
{
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

    Trade(Trade && other) noexcept = default;
    Trade(const Trade & other) = default;

public: // TODO make private with getters
        // private:
    std::chrono::milliseconds ts;
    std::string symbol;
    double price{};
    UnsignedVolume unsigned_volume;
    Side side = Side::Buy;
    double fee = 0.0;
};
