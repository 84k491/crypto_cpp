#include "PositionManager.h"

#include "Trade.h"
#include "Volume.h"

#include <iostream>
#include <optional>
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
        std::cout << "Not all volume was traded for opened position" << std::endl;
        return std::nullopt;
    }

    if (!m_closed_position.has_value()) {
        std::cout << "ERROR: No closed position when opened vol == 0" << std::endl;
    }
    const auto & closed_pos = m_closed_position.value();

    PositionResult res;
    res.pnl_with_fee = closed_pos.m_rpnl - pos.entry_fee();
    res.fees_paid += pos.entry_fee() + closed_pos.m_close_fee;
    res.opened_time = trade.ts() - pos.open_ts();

    m_opened_position = {};
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

double PositionManager::OpenedPosition::calc_rpnl_on_close(
        SignedVolume opening_volume,
        Vwap opening_vwap,
        Vwap closing_vwap,
        double close_fee) const
{
    auto res = closing_vwap.value() * opening_volume.value() - opening_vwap.value() * opening_volume.value() - close_fee * opening_volume.sign();
    res *= m_absolute_opened_volume.sign();
    return res;
}

std::optional<PositionManager::ClosedPosition> PositionManager::OpenedPosition::on_trade(double price, const SignedVolume & vol, double fee)
{
    if (vol.is_zero()) {
        std::cout << "ERROR! Zero volume on trade" << std::endl;
        return std::nullopt;
    }

    if (m_absolute_opened_volume.is_zero() || vol.sign() == m_absolute_opened_volume.sign()) {
        m_absolute_opened_volume += vol;
        m_opened_vwap += Vwap(vol.as_unsigned_and_side().first, price);
        m_entry_fee += fee;
        return std::nullopt;
    }

    ClosedPosition closed_pos;
    auto rpnl_on_close = calc_rpnl_on_close(vol * -1, m_opened_vwap, Vwap(vol.as_unsigned_and_side().first, price), fee);
    closed_pos.m_closed_volume = vol;
    closed_pos.m_closed_vwap = Vwap(vol.as_unsigned_and_side().first, price);
    m_opened_vwap -= closed_pos.m_closed_vwap;
    m_absolute_opened_volume += vol;
    closed_pos.m_rpnl = rpnl_on_close;
    closed_pos.m_close_fee = fee;
    return closed_pos;
}

PositionManager::ClosedPosition & PositionManager::ClosedPosition::operator+=(const ClosedPosition & other)
{
    m_closed_volume += other.m_closed_volume;
    m_closed_vwap += other.m_closed_vwap;
    m_rpnl += other.m_rpnl;
    m_close_fee += other.m_close_fee;
    return *this;
}
