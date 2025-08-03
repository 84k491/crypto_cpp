#pragma once

#include "Trade.h"
#include "Volume.h"

#include <crossguid/guid.hpp>

struct ProfitPriceLevels
{
    double fee_profit_price;
    // vvvvvvvv

    double no_loss_price;

    // ^^^^^^
    double fee_loss_price;
};

class ClosedPosition
{
    friend std::ostream & operator<<(std::ostream & os, const ClosedPosition & pos);

public:
    ClosedPosition() = default;

    ClosedPosition & operator+=(const ClosedPosition & other);

    SignedVolume m_closed_volume;
    double m_avg_closed_price = {};

    double m_rpnl = {}; // close fee included
    double m_entry_and_close_fee = {};
};

class OpenedPosition
{
    friend std::ostream & operator<<(std::ostream & os, const OpenedPosition & pos);

public:
    OpenedPosition(const Trade & trade);

    auto side() const { return m_absolute_volume.as_unsigned_and_side().second; }
    auto open_ts() const { return m_open_ts; }
    auto total_entry_fee() const { return m_total_entry_fee; } // for whole current opened vol
    auto opened_volume() const { return m_absolute_volume; }
    auto avg_entry_price() const { return m_avg_entry_price; }

    auto expected_total_fee() const { return m_total_entry_fee * 2; }
    ProfitPriceLevels price_levels() const;

    std::optional<ClosedPosition> on_trade(double price, const SignedVolume & vol, double fee);

    auto & guid() const { return m_guid; }

private:
    double price_for_upnl(double required_upnl) const;
    double extract_fee(const UnsignedVolume & vol);

private:
    xg::Guid m_guid;

    SignedVolume m_absolute_volume;
    double m_avg_entry_price = {};

    double m_total_entry_fee = {};

    std::chrono::milliseconds m_open_ts = {};
};
