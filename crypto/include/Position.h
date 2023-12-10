#pragma once

#include "Enums.h"

#include <chrono>
#include <optional>
#include <utility>

struct MarketOrder
{
    MarketOrder(double unsigned_volume, Side side);
    MarketOrder & operator+=(const MarketOrder & other);

private:
    double m_unsigned_volume{};
    Side m_side = Side::Buy;
};

struct PositionResult
{
public:
    double pnl = 0.;
    std::chrono::milliseconds opened_time = {};
};

class OpenedPosition // TODO make it private to Position
{
public:
    OpenedPosition(std::chrono::milliseconds ts, double absolute_volume, double price);

    double absolute_volume() const { return m_absolute_volume; }
    Side side() const;

    double m_absolute_volume = 0.;
    double m_open_price = 0.;
    std::chrono::milliseconds m_open_ts = {};
};

class Position
{
public:
    using PnlType = double;

    [[nodiscard]] std::pair<std::optional<MarketOrder>, std::optional<PositionResult>> open_or_move(
            std::chrono::milliseconds ts,
            double target_volume,
            double price);
    [[nodiscard]] std::optional<std::pair<MarketOrder, PositionResult>> close(
            std::chrono::milliseconds ts,
            double price);

    double pnl() const;
    OpenedPosition * opened() const;

private:
    std::optional<OpenedPosition> m_opened_position;

    std::chrono::milliseconds m_open_ts = {};
    std::chrono::milliseconds m_close_ts = {};
};
