#pragma once

#include "Volume.h"
#include "crossguid/guid.hpp"

#include <chrono>

class Trade
{
    friend std::ostream & operator<<(std::ostream & os, const Trade & trade);

public:
    Trade(
            std::chrono::milliseconds ts,
            std::string symbol_name,
            xg::Guid order_guid,
            double price,
            UnsignedVolume volume,
            Side side,
            double fee)
        : m_ts(ts)
        , m_symbol_name(std::move(symbol_name))
        , m_price(price)
        , m_unsigned_volume(std::move(volume))
        , m_side(side)
        , m_fee(fee)
        , m_order_guid(order_guid)
    {
    }

    std::chrono::milliseconds ts() const { return m_ts; }
    const auto & symbol_name() const { return m_symbol_name; }
    double price() const { return m_price; }
    UnsignedVolume unsigned_volume() const { return m_unsigned_volume; }
    SignedVolume signed_volume() const { return {m_unsigned_volume, m_side}; }
    Side side() const { return m_side; }
    double fee() const { return m_fee; }
    xg::Guid order_guid() const { return m_order_guid; }

private:
    std::chrono::milliseconds m_ts;
    std::string m_symbol_name;
    double m_price{};
    UnsignedVolume m_unsigned_volume;
    Side m_side = Side::buy();
    double m_fee = 0.0;
    xg::Guid m_order_guid;
};
std::ostream & operator<<(std::ostream & os, const Trade & trade);

class PublicTrade
{
public:
    PublicTrade(
            std::chrono::milliseconds ts,
            double price,
            SignedVolume volume)
        : m_ts(ts)
        , m_price(price)
        , m_signed_volume(volume)
    {
    }

    std::chrono::milliseconds ts() const { return m_ts; }
    double price() const { return m_price; }
    SignedVolume volume() const { return m_signed_volume; }

private:
    std::chrono::milliseconds m_ts;
    double m_price{};
    SignedVolume m_signed_volume;
};
