#pragma once

#include "Enums.h"
#include "Symbol.h"

#include <chrono>
#include <optional>
#include <utility>

struct MarketOrder
{
    MarketOrder(const std::string & symbol, double unsigned_volume, Side side);
    MarketOrder & operator+=(const MarketOrder & other);

    auto unsigned_volume() const { return m_unsigned_volume; }
    auto symbol() const { return m_symbol; }
    std::string side_str() const;

private:
    std::string m_symbol;
    double m_unsigned_volume{};
    Side m_side = Side::Buy;
};

struct PositionResult
{
public:
    double pnl = 0.;
    std::chrono::milliseconds opened_time = {};
};

class Position
{
    class OpenedPosition
    {
    public:
        OpenedPosition(std::chrono::milliseconds ts, double absolute_volume, double price);

        Side side() const;

        auto absolute_volume() const { return m_absolute_volume; }
        auto open_price() const { return m_open_price; }
        auto open_ts() const { return m_open_ts; }

    private:
        double m_absolute_volume = 0.;
        double m_open_price = 0.;
        std::chrono::milliseconds m_open_ts = {};
    };

public:
    Position(Symbol symbol)
        : m_symbol(std::move(symbol))
    {
    }

    [[nodiscard]] std::pair<std::optional<MarketOrder>, std::optional<PositionResult>> open_or_move(
            std::chrono::milliseconds ts,
            double target_volume,
            double price);
    [[nodiscard]] std::optional<std::pair<MarketOrder, PositionResult>> close(
            std::chrono::milliseconds ts,
            double price);

    double pnl() const;
    const OpenedPosition * opened() const
    {
        return m_opened_position.has_value() ? &m_opened_position.value() : nullptr;
    }

private:
    std::optional<OpenedPosition> m_opened_position;
    const Symbol m_symbol;
};
