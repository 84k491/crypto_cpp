#pragma once

#include "Enums.h"
#include "Symbol.h"
#include "Volume.h"
#include "Vwap.h"

#include <chrono>
#include <expected>
#include <optional>
#include <utility>

class Trade;
struct PositionResult
{
    friend std::ostream & operator<<(std::ostream & os, const PositionResult & res);

public:
    double fees_paid = 0.;
    double pnl_with_fee = 0.;
    std::chrono::milliseconds opened_time = {};
};
std::ostream & operator<<(std::ostream & os, const PositionResult & res);

class PositionManager
{
    class ClosedPosition
    {
    public:
        ClosedPosition() {}

        ClosedPosition & operator+=(const ClosedPosition & other);

        SignedVolume m_closed_volume = {};
        Vwap m_closed_vwap;

        double m_rpnl = {};
        double m_close_fee = {};
    };

    class OpenedPosition
    {
    public:
        OpenedPosition(const Trade & trade);

        auto side() const { return m_absolute_opened_volume.as_unsigned_and_side().second; }
        auto open_ts() const { return m_open_ts; }
        auto entry_fee() const { return m_entry_fee; }
        auto opened_volume() const { return m_absolute_opened_volume; }

        std::optional<ClosedPosition> on_trade(double price, const SignedVolume & vol, double fee);

    private:
        double calc_rpnl_on_close(
                SignedVolume opening_volume,
                Vwap opening_vwap,
                Vwap closing_vwap,
                double close_fee) const;

    private:
        SignedVolume m_absolute_opened_volume = {}; // TODO rename to just abs_vol
        Vwap m_opened_vwap;
        double m_entry_fee = {};
        std::chrono::milliseconds m_open_ts = {};
    };

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
