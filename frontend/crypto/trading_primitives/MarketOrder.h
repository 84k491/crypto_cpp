#pragma once

#include "Volume.h"

#include <chrono>
#include <crossguid/guid.hpp>
#include <ostream>

class MarketOrder
{
    friend std::ostream & operator<<(std::ostream & os, const MarketOrder & order);

public:
    enum class Status : uint8_t
    {
        Pending,
        // PartiallyFilled,
        Filled,
        Rejected,
    };

    // TODO remove one of the c-tors
    MarketOrder(
            std::string symbol,
            double price,
            UnsignedVolume volume,
            Side side,
            std::chrono::milliseconds signal_ts)
        : m_guid(xg::newGuid())
        , m_symbol(std::move(symbol))
        , m_volume(std::move(volume))
        , m_side(side)
        , m_price(price)
        , m_signal_ts(signal_ts)
    {
    }

    MarketOrder(
            std::string symbol,
            double price,
            SignedVolume signed_volume,
            std::chrono::milliseconds signal_ts)
        : m_guid(xg::newGuid())
        , m_symbol(std::move(symbol))
        , m_volume(signed_volume.as_unsigned_and_side().first)
        , m_side(signed_volume.as_unsigned_and_side().second)
        , m_price(price)
        , m_signal_ts(signal_ts)
    {
    }

    std::string side_str() const // TODO remove this method, just print side
    {
        return m_side.to_string();
    }

    void regenerate_guid() { m_guid = xg::newGuid(); }
    auto target_volume() const { return m_target_volume; }
    auto filled_volume() const { return m_filled_volume; }
    auto volume() const { return m_volume; }
    auto symbol() const { return m_symbol; }
    auto side() const { return m_side; }
    auto price() const { return m_price; }
    auto signal_ts() const { return m_signal_ts; }
    auto guid() const { return m_guid; }

    const auto & reject_reason() const { return m_reject_reason; }
    void set_reject_reason(const std::string & r) { m_reject_reason = r; }

    Status status() const;

    void on_trade(UnsignedVolume vol, double price, double fee);

    std::string m_reject_reason;

private:
    xg::Guid m_guid;
    std::string m_symbol;
    UnsignedVolume m_target_volume; // eventually -> active(always 0) + filled
    UnsignedVolume m_filled_volume;
    UnsignedVolume m_volume; // TODO remove
    double m_fee = 0;

    Side m_side = Side::buy();
    double m_price = 0.;
    std::chrono::milliseconds m_signal_ts = {};
};
std::ostream & operator<<(std::ostream & os, const MarketOrder & order);
