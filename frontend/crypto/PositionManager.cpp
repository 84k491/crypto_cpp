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
        Logger::logf<LogLevel::Status>("Opened position: {}", m_opened_position.value());
        return std::nullopt;
    }

    auto & pos = m_opened_position.value();
    const auto trade_vol = SignedVolume(trade.unsigned_volume(), trade.side());
    const auto closed_pos_opt = pos.on_trade(trade.price(), trade_vol, trade.fee());
    if (closed_pos_opt.has_value()) {
        Logger::logf<LogLevel::Status>("Closed position: {}", closed_pos_opt.value());
        if (m_closed_position.has_value()) {
            m_closed_position.value() += *closed_pos_opt;
        }
        else {
            m_closed_position = *closed_pos_opt;
        }
    }
    if (!pos.opened_volume().is_zero()) {
        Logger::logf<LogLevel::Status>("New opened position state: {}", pos);
        return std::nullopt;
    }
    Logger::logf<LogLevel::Status>("Position {} has been closed completely", pos.guid());

    if (!m_closed_position.has_value()) {
        Logger::log<LogLevel::Error>("ERROR: No closed position when opened vol == 0");
        return std::nullopt;
    }
    const auto & closed_pos = m_closed_position.value();

    PositionResult res;
    res.pnl_with_fee = closed_pos.m_rpnl - pos.entry_fee();
    res.fees_paid += pos.entry_fee() + closed_pos.m_close_fee;
    res.open_ts = pos.open_ts();
    res.close_ts = trade.ts();
    res.guid = pos.guid();

    m_opened_position = {};
    m_closed_position = {};

    Logger::logf<LogLevel::Status>("Position result: {}", res);
    return res;
}

std::chrono::milliseconds PositionResult::opened_time() const
{
    return (close_ts - open_ts);
}

std::ostream & operator<<(std::ostream & os, const PositionResult & res)
{
    os << "PositionResult: "
       << "pnl_with_fee = " << res.pnl_with_fee
       << ", fees_paid = " << res.fees_paid
       << ", opened_time = " << res.opened_time();
    return os;
}
