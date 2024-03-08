#pragma once

#include "Enums.h"
#include "ITradingGateway.h"
#include "Symbol.h"
#include "Volume.h"

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

// TODO rename this file to position manager
class PositionManager
{
    class OpenedPosition
    {
    public:
        OpenedPosition(const Trade & trade);

        Side side() const;

        auto absolute_volume() const { return m_absolute_volume; }
        auto open_ts() const { return m_open_ts; }
        double pnl() const;
        auto total_fee() const { return m_total_fee; }

        void on_trade(double price, const SignedVolume & vol, double fee);

    private:
        SignedVolume m_absolute_volume = {}; // TODO remove?

        Side m_open_side = Side::Buy;

        UnsignedVolume m_opened_volume = {};
        double m_opened_vwap = 0.;

        UnsignedVolume m_closed_volume = {};
        double m_closed_vwap = 0.;

        std::chrono::milliseconds m_open_ts = {};

        double m_total_fee = 0.;
    };

public:
    PositionManager(Symbol symbol, ITradingGateway & tr_gateway)
        : m_symbol(std::move(symbol))
        , m_tr_gateway(tr_gateway)
    {
    }

    // TODO position result publisher?

    std::optional<PositionResult> on_trade_received(const Trade & trade);

    double pnl() const;
    const OpenedPosition * opened() const
    {
        return m_opened_position.has_value() ? &m_opened_position.value() : nullptr;
    }

private:
    std::optional<OpenedPosition> m_opened_position;
    const Symbol m_symbol;

    ITradingGateway & m_tr_gateway;
};
