#include "Position.h"

#include <iostream>

MarketOrder Position::open(std::chrono::milliseconds ts, Side side, double size, double price)
{
    m_size = size;
    m_open_price = price;
    m_open_ts = ts;
    m_side = side;

    return MarketOrder{.m_size = size, .m_side = side};
}

MarketOrder Position::close(std::chrono::milliseconds ts, double price)
{
    const auto order_side = m_side == Side::Buy ? Side::Sell : Side::Buy;

    m_close_price = price;
    m_close_ts = ts;

    return MarketOrder{.m_size = m_size, .m_side = order_side};
}

double Position::pnl() const
{
    if (opened()) {
        return 0;
    }

    const int sign = m_side == Side::Buy ? 1 : -1;
    return (m_close_price.value() - m_open_price) * m_size * sign;
}

double Position::volume() const
{
    return m_size;
}

bool Position::opened() const
{
    return !m_close_price.has_value();
}

Side Position::side_from_absolute_volume(double absolute_volume)
{
    if (absolute_volume > 0) {
        return Side::Buy;
    }
    return Side::Sell;
}

MarketOrder & MarketOrder::operator+=(const MarketOrder & other)
{
    m_size += other.m_size;
    return *this;
}

Side Position::side() const
{
    return m_side;
}
