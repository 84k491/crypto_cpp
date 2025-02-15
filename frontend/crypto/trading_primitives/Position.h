#pragma once

#include "Trade.h"
#include "Volume.h"

struct ProfitPriceLevels
{
    // double profit;

    double fee_profit_price;
    // vvvvvvvv

    double no_loss_price;

    // ^^^^^^
    double fee_loss_price;

    // double loss;
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
    double m_close_fee = {};
};

class OpenedPosition
{
    friend std::ostream & operator<<(std::ostream & os, const OpenedPosition & pos);
public:
    OpenedPosition(const Trade & trade);

    auto side() const { return m_absolute_volume.as_unsigned_and_side().second; }
    auto open_ts() const { return m_open_ts; }
    auto entry_fee() const { return m_entry_fee; }
    auto opened_volume() const { return m_absolute_volume; }
    auto avg_entry_price() const { return m_avg_entry_price; }

    std::optional<ClosedPosition> on_trade(double price, const SignedVolume & vol, double fee);

private:
    SignedVolume m_absolute_volume;
    double m_avg_entry_price = {};
    double m_entry_fee = {};
    std::chrono::milliseconds m_open_ts = {};
};
