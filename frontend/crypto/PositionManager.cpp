#include "PositionManager.h"

#include "Logger.h"
#include "Trade.h"
#include "Volume.h"

#include <iostream>
#include <optional>

std::optional<PositionResult> PositionManager::on_trade_received(const Trade & trade)
{
    if (!m_opened_position.has_value()) {
        m_opened_position = OpenedPosition(trade);
        LOG_STATUS("Opened position: {}", m_opened_position.value());
        return std::nullopt;
    }

    auto & pos = m_opened_position.value();
    const auto trade_vol = SignedVolume(trade.unsigned_volume(), trade.side());
    const auto closed_pos_opt = pos.on_trade(trade.price(), trade_vol, trade.fee());

    if (!closed_pos_opt.has_value()) {
        LOG_STATUS("Position opened further. New state: {}", pos);
        return {};
    }

    LOG_STATUS("Closed position: {}", *closed_pos_opt);
    {
        if (m_closed_position.has_value()) {
            m_closed_position.value() += *closed_pos_opt;
        }
        else {
            m_closed_position = *closed_pos_opt;
        }
    }

    const auto & closed_pos = m_closed_position.value();

    PositionResult res;
    res.pnl_with_fee = closed_pos.m_rpnl;
    res.fees_paid += closed_pos.m_entry_and_close_fee;
    res.open_ts = pos.open_ts();
    res.guid = pos.guid();

    if (pos.opened_volume().is_zero()) {
        LOG_STATUS("Position {} has been closed completely", pos.guid());
        res.close_ts = trade.ts();
        m_opened_position = {};
        m_closed_position = {};
    }
    else {
        LOG_STATUS("Position partially closed. Now: {}", pos);
    }

    LOG_STATUS("Position result: {}", res);
    return res;
}

std::optional<std::chrono::milliseconds> PositionResult::opened_time() const
{
    if (!close_ts.has_value()) {
        return {};
    }
    const auto res = (close_ts.value() - open_ts);
    return res;
}

std::ostream & operator<<(std::ostream & os, const PositionResult & res)
{
    os << "PositionResult: "
       << "pnl_with_fee = " << res.pnl_with_fee
       << ", fees_paid = " << res.fees_paid
       << ", opened_time = " << (res.opened_time().has_value() ? std::to_string(res.opened_time()->count()) : "<empty>");
    return os;
}
