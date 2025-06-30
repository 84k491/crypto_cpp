#include "MarketOrder.h"

std::ostream & operator<<(std::ostream & os, const MarketOrder & order)
{
    os << "MarketOrder: " << order.side_str() << "; target_volume: " << order.target_volume() << "; symbol: " << order.symbol();
    return os;
}

void MarketOrder::on_trade(UnsignedVolume vol, double price, double fee)
{
    m_filled_volume += vol;
    // TODO it must be an 'average trade price'
    m_price = price;
    m_fee += fee;
}

OrderStatus MarketOrder::status() const
{
    if (!m_reject_reason.empty()) {
        return OrderStatus::Rejected;
    }

    if (m_filled_volume < m_target_volume) {
        return OrderStatus::Pending;
    }

    if (m_target_volume <= m_filled_volume) {
        return OrderStatus::Filled;
    }

    return OrderStatus::Rejected;
}
