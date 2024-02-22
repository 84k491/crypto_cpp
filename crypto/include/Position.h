#pragma once

#include "Enums.h"
#include "ITradingGateway.h"
#include "Symbol.h"
#include "TradingPrimitives.h"
#include "Types.h"

#include <chrono>
#include <expected>
#include <optional>
#include <utility>

struct PositionResult
{
public:
    double pnl = 0.;
    std::chrono::milliseconds opened_time = {};
};

// TODO rename this file to position manager
class PositionManager
{
    class OpenedPosition
    {
    public:
        OpenedPosition(std::chrono::milliseconds ts, SignedVolume absolute_volume, double price);

        Side side() const;

        auto absolute_volume() const { return m_absolute_volume; }
        auto open_price() const { return m_open_price; }
        auto open_ts() const { return m_open_ts; }

    private:
        SignedVolume m_absolute_volume;
        double m_open_price = 0.;
        std::chrono::milliseconds m_open_ts = {};
    };

public:
    PositionManager(Symbol symbol, ITradingGateway * tr_gateway)
        : m_symbol(std::move(symbol))
        , m_tr_gateway(tr_gateway)
    {
    }

    // TODO make signed and unsigned volume types
    bool open(SignedVolume target_absolute_volume);
    std::optional<PositionResult> close();

    std::optional<PositionResult> on_trade(const Trade & trade);

    double pnl() const;
    const OpenedPosition * opened() const
    {
        return m_opened_position.has_value() ? &m_opened_position.value() : nullptr;
    }

private:
    std::optional<OpenedPosition> m_opened_position;
    const Symbol m_symbol;

    ITradingGateway * m_tr_gateway = nullptr;
};
