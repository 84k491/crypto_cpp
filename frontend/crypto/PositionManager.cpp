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
    const auto trade_vol = SignedVolume(trade.unsigned_volume, trade.side);
    pos.on_trade(trade.price, trade_vol, trade.fee);
    if (!pos.absolute_volume().is_zero()) {
        std::cout << "Not all volume was traded for opened position" << std::endl;
        return std::nullopt;
    }

    PositionResult res;
    res.pnl_with_fee = pos.pnl() - pos.total_fee();
    res.fees_paid += pos.total_fee();
    res.opened_time = trade.ts - pos.open_ts();

    m_opened_position = {};
    return res;
}

Side PositionManager::OpenedPosition::side() const
{
    return m_absolute_volume.as_unsigned_and_side().second;
}

PositionManager::OpenedPosition::OpenedPosition(const Trade & trade)
    : m_open_side(trade.side)
    , m_open_ts(trade.ts)
{
    on_trade(trade.price, SignedVolume(trade.unsigned_volume, trade.side), trade.fee);
}

std::ostream & operator<<(std::ostream & os, const PositionResult & res)
{
    os << "PositionResult: "
       << "pnl_with_fee = " << res.pnl_with_fee
       << ", fees_paid = " << res.fees_paid
       << ", opened_time = " << res.opened_time;
    return os;
}

void PositionManager::OpenedPosition::on_trade(double price, const SignedVolume & vol, double fee)
{
    // TODO cover with test, refactor
    const auto trade_side = vol.as_unsigned_and_side().second;
    auto & volume = trade_side == m_open_side ? m_opened_volume : m_closed_volume;
    auto & vwap = trade_side == m_open_side ? m_opened_vwap : m_closed_vwap;

    const UnsignedVolume new_volume = volume + vol.as_unsigned_and_side().first;
    vwap = ((vwap * volume.value()) + (price * vol.as_unsigned_and_side().first.value())) / new_volume.value();
    volume = new_volume;

    m_total_fee += fee;
    m_absolute_volume += vol;
}

double PositionManager::OpenedPosition::pnl() const
{
    auto res = m_closed_vwap * m_closed_volume.value() - m_opened_vwap * m_opened_volume.value();
    int sign = m_open_side == Side::Buy ? 1 : -1;
    res *= sign;
    return res;
}
