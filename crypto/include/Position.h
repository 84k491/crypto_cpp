#pragma once

#include "Enums.h"

#include <chrono>
#include <optional>

struct MarketOrder
{
    MarketOrder & operator+=(const MarketOrder & other);

    double m_size{};
    Side m_side = Side::Buy;
};

class Position
{
public:
    [[nodiscard]] MarketOrder open(std::chrono::milliseconds ts, Side side, double size, double price);
    [[nodiscard]] MarketOrder close(std::chrono::milliseconds ts, double price);

    static Side side_from_absolute_volume(double absolute_volume);

    double pnl() const;
    double volume() const;
    bool opened() const;
    Side side() const;

private:
    Side m_side = Side::Buy;

    double m_size = 0.;
    double m_open_price = 0.;
    std::optional<double> m_close_price = {};
    std::chrono::milliseconds m_open_ts = {};
    std::chrono::milliseconds m_close_ts = {};
};
