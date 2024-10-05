#include "Position.h"

#include "Logger.h"

OpenedPosition::OpenedPosition(const Trade & trade)
    : m_open_ts(trade.ts())
{
    on_trade(trade.price(), SignedVolume(trade.unsigned_volume(), trade.side()), trade.fee());
}

std::optional<ClosedPosition> OpenedPosition::on_trade(double price, const SignedVolume & vol, double fee)
{
    if (vol.is_zero()) {
        Logger::log<LogLevel::Error>("Zero volume on trade");
        return std::nullopt;
    }

    // open or increase
    if (m_absolute_volume.is_zero() || vol.sign() == m_absolute_volume.sign()) {
        m_absolute_volume += vol;
        m_currency_amount += price * -1 * vol.value();

        m_entry_fee += fee;
        return std::nullopt;
    }

    ClosedPosition closed_pos;
    closed_pos.m_currency_amount = price * -1 * vol.value();
    closed_pos.m_closed_volume = vol;
    closed_pos.m_close_fee = fee;

    const auto old_opened_currency_amount = m_currency_amount;
    m_currency_amount -= -vol.sign() * (m_currency_amount / m_absolute_volume.value()) * vol.as_unsigned_and_side().first.value();
    const auto entry_amount_closed = (old_opened_currency_amount - m_currency_amount);
    auto raw_profit_on_close = (closed_pos.m_currency_amount + entry_amount_closed);
    m_absolute_volume += vol;
    closed_pos.m_rpnl = raw_profit_on_close - fee;

    return closed_pos;
}

ClosedPosition & ClosedPosition::operator+=(const ClosedPosition & other)
{
    m_closed_volume += other.m_closed_volume;
    m_currency_amount += other.m_currency_amount;
    m_rpnl += other.m_rpnl;
    m_close_fee += other.m_close_fee;
    return *this;
}

std::ostream & operator<<(std::ostream & os, const OpenedPosition & pos)
{
    os << "OpenedPosition: "
       << "side = " << pos.side()
       << ", open_ts = " << pos.open_ts()
       << ", entry_fee = " << pos.entry_fee()
       << ", opened_volume = " << pos.opened_volume().as_unsigned_and_side().first;
    return os;
}

std::ostream & operator<<(std::ostream & os, const ClosedPosition & pos)
{
    os << "ClosedPosition: "
       << "closed_volume = " << pos.m_closed_volume.value()
       << ", currency_amount = " << pos.m_currency_amount
       << ", rpnl = " << pos.m_rpnl
       << ", close_fee = " << pos.m_close_fee;
    return os;
}
