#include "Position.h"

#include "Logger.h"
#include "ScopeExit.h"

OpenedPosition::OpenedPosition(const Trade & trade)
    : m_single_trade_fee(trade.fee())
    , m_open_ts(trade.ts())
{
    on_trade(trade.price(), SignedVolume(trade.unsigned_volume(), trade.side()), trade.fee());
}

std::optional<ClosedPosition> OpenedPosition::on_trade(double price, const SignedVolume & vol, double fee)
{
    if (vol.is_zero()) {
        Logger::log<LogLevel::Error>("Zero volume on trade");
        return std::nullopt;
    }

    ScopeExit se{[&]() {
        m_absolute_volume += vol;
    }};

    // open or increase
    if (m_absolute_volume.is_zero() || vol.sign() == m_absolute_volume.sign()) {
        const auto amount_before_trade = m_avg_entry_price * m_absolute_volume.value();
        const auto trade_amount = price * vol.value();

        m_avg_entry_price = (amount_before_trade + trade_amount) / (m_absolute_volume.value() + vol.value());
        m_total_entry_fee += fee;
        return std::nullopt;
    }

    ClosedPosition closed_pos;
    closed_pos.m_avg_closed_price = price;
    closed_pos.m_closed_volume = vol;
    closed_pos.m_close_fee = fee;
    const auto raw_profit_on_close = (price - m_avg_entry_price) * -vol.value();
    closed_pos.m_rpnl = raw_profit_on_close - fee;

    return closed_pos;
}

ClosedPosition & ClosedPosition::operator+=(const ClosedPosition & other)
{
    m_closed_volume += other.m_closed_volume;
    const auto current_amount = m_closed_volume.value() * m_avg_closed_price;
    const auto other_amount = other.m_closed_volume.value() * other.m_avg_closed_price;
    m_avg_closed_price = (current_amount + other_amount) / (m_closed_volume.value() + other.m_closed_volume.value());
    m_rpnl += other.m_rpnl;
    m_close_fee += other.m_close_fee;
    return *this;
}

double OpenedPosition::upnl(double current_price) const
{
    return ((current_price - m_avg_entry_price) * m_absolute_volume.value()) - expected_total_fee();
}

double OpenedPosition::price_for_upnl(double required_upnl) const
{
    return m_avg_entry_price + ((required_upnl + expected_total_fee()) / m_absolute_volume.value());
}

ProfitPriceLevels OpenedPosition::price_levels() const
{
    double fee_profit_price = price_for_upnl(expected_total_fee());
    double no_loss_price = price_for_upnl(0.0);
    double fee_loss_price = price_for_upnl(-expected_total_fee());

    return {.fee_profit_price=fee_profit_price, .no_loss_price=no_loss_price, .fee_loss_price=fee_loss_price};
}

std::ostream & operator<<(std::ostream & os, const OpenedPosition & pos)
{
    os << "OpenedPosition: "
       << "side = " << pos.side()
       << ", open_ts = " << pos.open_ts()
       << ", entry_fee = " << pos.entry_fee()
       << ", opened_volume = " << pos.opened_volume().as_unsigned_and_side().first
       << ", avg_entry_price = " << pos.m_avg_entry_price;
    return os;
}

std::ostream & operator<<(std::ostream & os, const ClosedPosition & pos)
{
    os << "ClosedPosition: "
       << "closed_volume = " << pos.m_closed_volume.value()
       << ", avg_closed_price = " << pos.m_avg_closed_price
       << ", rpnl = " << pos.m_rpnl
       << ", close_fee = " << pos.m_close_fee;
    return os;
}
