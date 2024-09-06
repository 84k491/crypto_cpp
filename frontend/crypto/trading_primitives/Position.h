#pragma once

#include "Trade.h"
#include "Volume.h"

class ClosedPosition
{
public:
    ClosedPosition() = default;

    ClosedPosition & operator+=(const ClosedPosition & other);

    SignedVolume m_closed_volume;
    // Vwap m_closed_vwap;
    double m_currency_amount = {};

    double m_rpnl = {};
    double m_close_fee = {};
};

class OpenedPosition
{
public:
    OpenedPosition(const Trade & trade);

    auto side() const { return m_absolute_opened_volume.as_unsigned_and_side().second; }
    auto open_ts() const { return m_open_ts; }
    auto entry_fee() const { return m_entry_fee; }
    auto opened_volume() const { return m_absolute_opened_volume; }

    std::optional<ClosedPosition> on_trade(double price, const SignedVolume & vol, double fee);

private:
    SignedVolume m_absolute_opened_volume; // TODO rename to just abs_vol
    // Vwap m_opened_vwap;
    double m_currency_amount = {};
    double m_entry_fee = {};
    std::chrono::milliseconds m_open_ts = {};
};
