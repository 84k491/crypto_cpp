#include "Position.h"

#include <iostream>

Side side_from_absolute_volume(double absolute_volume)
{
    if (absolute_volume > 0) {
        return Side::Buy;
    }
    return Side::Sell;
}

std::pair<std::optional<MarketOrder>, std::optional<PositionResult>> Position::open_or_move(std::chrono::milliseconds ts, double target_volume, double price)
{
    if (0. == target_volume) {
        std::cout << "ERROR: closing on open_or_move" << std::endl;
        return {std::nullopt, std::nullopt};
    }

    if (m_opened_position.has_value()) {
        if (m_opened_position.value().absolute_volume() == target_volume) {
            return {std::nullopt, std::nullopt};
        }

        if (m_opened_position.value().side() == side_from_absolute_volume(target_volume)) {
            // just moving the position
            const auto volume_delta = target_volume - m_opened_position.value().absolute_volume();
            return {MarketOrder{
                            std::abs(volume_delta),
                            side_from_absolute_volume(volume_delta)},
                    std::nullopt};
        }
        // close current and open a new one
        auto order_and_res_opt = close(ts, price);
        if (!order_and_res_opt.has_value()) {
            std::cout << "ERROR: no optional" << std::endl;
            return {std::nullopt, std::nullopt};
        }
        auto & [order, res] = order_and_res_opt.value();
        m_opened_position = OpenedPosition(ts, target_volume, price);
        const auto open_order = MarketOrder{
                std::abs(target_volume),
                side_from_absolute_volume(target_volume)};
        order += open_order;
        return {order, res};
    }

    m_opened_position = OpenedPosition(ts, target_volume, price);
    return {MarketOrder{
                    std::abs(target_volume),
                    side_from_absolute_volume(target_volume)},
            std::nullopt};
}

std::optional<std::pair<MarketOrder, PositionResult>> Position::close(
        std::chrono::milliseconds ts,
        double price)
{
    if (!m_opened_position.has_value()) {
        return std::nullopt;
    }
    auto & pos = m_opened_position.value();

    PositionResult res;
    const auto pnl = (price - pos.m_open_price) * pos.m_absolute_volume;
    res.pnl = pnl;
    res.opened_time = ts - m_open_ts;

    const auto close_order = MarketOrder{
            std::abs(pos.m_absolute_volume),
            side_from_absolute_volume(-pos.m_absolute_volume)};

    m_opened_position = {};
    return {{close_order, res}};
}

MarketOrder & MarketOrder::operator+=(const MarketOrder & other)
{
    m_unsigned_volume += other.m_unsigned_volume;
    return *this;
}

MarketOrder::MarketOrder(double unsigned_volume, Side side)
    : m_unsigned_volume(unsigned_volume)
    , m_side(side)
{
}

Side OpenedPosition::side() const
{
    return side_from_absolute_volume(m_absolute_volume);
}

OpenedPosition::OpenedPosition(std::chrono::milliseconds ts, double absolute_volume, double price)
    : m_absolute_volume(absolute_volume)
    , m_open_ts(ts)
    , m_open_price(price)
{
}
