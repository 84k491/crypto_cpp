#include "PositionManager.h"

#include "Trade.h"
#include "Volume.h"

#include <iostream>
#include <optional>
#include <utility>

std::optional<PositionResult> PositionManager::on_trade_received(const Trade & trade)
{
    if (!m_opened_position.has_value()) {
        m_opened_position = OpenedPosition(trade.ts, SignedVolume(trade.unsigned_volume, trade.side), trade.price);
        return std::nullopt;
    }

    auto & pos = m_opened_position.value();
    const auto trade_vol = SignedVolume(trade.unsigned_volume, trade.side);
    pos.add_volume(trade_vol);
    if (!pos.absolute_volume().is_zero()) {
        // TODO must there be a result update after any volume is traded?
        std::cout << "Not all volume was traded for opened position" << std::endl;
        return std::nullopt;
    }

    PositionResult res;
    const auto pnl = (trade.price - pos.open_price()) * pos.absolute_volume().value();
    res.pnl = pnl;
    res.pnl -= trade.fee;
    res.fees_paid += trade.fee;
    res.opened_time = trade.ts - pos.open_ts();

    m_opened_position = {};
    return res;
}

Side PositionManager::OpenedPosition::side() const
{
    return m_absolute_volume.as_unsigned_and_side().second;
}

PositionManager::OpenedPosition::OpenedPosition(std::chrono::milliseconds ts, SignedVolume absolute_volume, double price)
    : m_absolute_volume(absolute_volume)
    , m_open_price(price)
    , m_open_ts(ts)
{
}

std::ostream & operator<<(std::ostream & os, const PositionResult & res)
{
    os << "PositionResult: "
       << "pnl = " << res.pnl
       << ", fees_paid = " << res.fees_paid
       << ", opened_time = " << res.opened_time;
    return os;
}
