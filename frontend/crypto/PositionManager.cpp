#include "PositionManager.h"

#include "Logger.h"
#include "Trade.h"
#include "Volume.h"

#include <iostream>
#include <optional>
#include <print>
#include <utility>

std::optional<PositionResult> PositionManager::on_trade_received(const Trade & trade)
{
    if (!m_opened_position.has_value()) {
        m_opened_position = OpenedPosition(trade);
        return std::nullopt;
    }

    auto & pos = m_opened_position.value();
    const auto trade_vol = SignedVolume(trade.unsigned_volume(), trade.side());
    const auto closed_pos_opt = pos.on_trade(trade.price(), trade_vol, trade.fee());
    if (closed_pos_opt.has_value()) {
        if (m_closed_position.has_value()) {
            m_closed_position.value() += *closed_pos_opt;
        }
        else {
            m_closed_position = *closed_pos_opt;
        }
    }
    if (!pos.opened_volume().is_zero()) {
        return std::nullopt;
    }

    if (!m_closed_position.has_value()) {
        Logger::log<LogLevel::Error>("ERROR: No closed position when opened vol == 0");
    }
    const auto & closed_pos = m_closed_position.value();

    PositionResult res;
    res.pnl_with_fee = closed_pos.m_rpnl - pos.entry_fee();
    res.fees_paid += pos.entry_fee() + closed_pos.m_close_fee;
    res.opened_time = trade.ts() - pos.open_ts();

    m_opened_position = {};
    m_closed_position = {};
    return res;
}

PositionManager::OpenedPosition::OpenedPosition(const Trade & trade)
    : m_open_ts(trade.ts())
{
    on_trade(trade.price(), SignedVolume(trade.unsigned_volume(), trade.side()), trade.fee());
}

std::ostream & operator<<(std::ostream & os, const PositionResult & res)
{
    os << "PositionResult: "
       << "pnl_with_fee = " << res.pnl_with_fee
       << ", fees_paid = " << res.fees_paid
       << ", opened_time = " << res.opened_time;
    return os;
}

std::optional<PositionManager::ClosedPosition> PositionManager::OpenedPosition::on_trade(double price, const SignedVolume & vol, double fee)
{
    if (vol.is_zero()) {
        Logger::log<LogLevel::Error>("Zero volume on trade");
        return std::nullopt;
    }

    // open or increase
    if (m_absolute_opened_volume.is_zero() || vol.sign() == m_absolute_opened_volume.sign()) {
        m_absolute_opened_volume += vol;
        m_currency_amount += price * -1 * vol.value();

        m_entry_fee += fee;
        return std::nullopt;
    }

    ClosedPosition closed_pos;
    closed_pos.m_currency_amount = price * -1 * vol.value();
    closed_pos.m_closed_volume = vol;
    closed_pos.m_close_fee = fee;

    const auto old_opened_currency_amount = m_currency_amount;
    m_currency_amount -= -vol.sign() * (m_currency_amount / m_absolute_opened_volume.value()) * vol.as_unsigned_and_side().first.value();
    const auto entry_amount_closed = (old_opened_currency_amount - m_currency_amount);
    auto raw_profit_on_close = (closed_pos.m_currency_amount + entry_amount_closed);
    m_absolute_opened_volume += vol;
    closed_pos.m_rpnl = raw_profit_on_close - fee;

    return closed_pos;
}

PositionManager::ClosedPosition & PositionManager::ClosedPosition::operator+=(const ClosedPosition & other)
{
    m_closed_volume += other.m_closed_volume;
    m_currency_amount += other.m_currency_amount;
    m_rpnl += other.m_rpnl;
    m_close_fee += other.m_close_fee;
    return *this;
}
