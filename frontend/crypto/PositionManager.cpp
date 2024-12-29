#include "PositionManager.h"

#include "Logger.h"
#include "Trade.h"
#include "Volume.h"

#include <iostream>
#include <optional>
#include <print>

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
    Logger::logf<LogLevel::Status>("Position has been closed completely");

    if (!m_closed_position.has_value()) {
        Logger::log<LogLevel::Error>("ERROR: No closed position when opened vol == 0");
        return std::nullopt;
    }
    const auto & closed_pos = m_closed_position.value();

    PositionResult res;
    res.pnl_with_fee = closed_pos.m_rpnl - pos.entry_fee();
    res.fees_paid += pos.entry_fee() + closed_pos.m_close_fee;
    res.opened_time = trade.ts() - pos.open_ts();

    m_opened_position = {};
    m_closed_position = {};

    Logger::logf<LogLevel::Status>("Position result: {}", res);
    return res;
}

std::ostream & operator<<(std::ostream & os, const PositionResult & res)
{
    os << "PositionResult: "
       << "pnl_with_fee = " << res.pnl_with_fee
       << ", fees_paid = " << res.fees_paid
       << ", opened_time = " << res.opened_time;
    return os;
}
