#pragma once

#include "Position.h"
#include "Symbol.h"

#include <chrono>
#include <expected>
#include <optional>
#include <utility>

class Trade;
struct PositionResult
{
    friend std::ostream & operator<<(std::ostream & os, const PositionResult & res);

public:
    std::chrono::milliseconds opened_time() const;

    xg::Guid guid;
    double fees_paid = 0.;
    double pnl_with_fee = 0.;
    std::chrono::milliseconds open_ts = {};
    std::chrono::milliseconds close_ts = {};
};
std::ostream & operator<<(std::ostream & os, const PositionResult & res);

class PositionManager
{
public:
    PositionManager(Symbol symbol)
        : m_symbol(std::move(symbol))
    {
    }

    std::optional<PositionResult> on_trade_received(const Trade & trade);

    const OpenedPosition * opened() const
    {
        return m_opened_position.has_value() ? &m_opened_position.value() : nullptr;
    }

private:
    std::optional<OpenedPosition> m_opened_position;
    std::optional<ClosedPosition> m_closed_position;
    const Symbol m_symbol;
};
