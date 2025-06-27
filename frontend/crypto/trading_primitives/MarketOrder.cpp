#include "MarketOrder.h"

std::ostream & operator<<(std::ostream & os, const MarketOrder & order)
{
    os << "MarketOrder: " << order.side_str() << "; volume: " << order.volume() << "; symbol: " << order.symbol();
    return os;
}

void MarketOrder::on_trade(UnsignedVolume v, double p)
{
    m_filled_volume += v;
    // TODO it must be an 'average trade price'
    m_price = p;
}

MarketOrder::Status MarketOrder::status() const
{
    if (!m_reject_reason.empty()) {
        return MarketOrder::Status::Rejected;
    }

    if (m_filled_volume < m_target_volume) {
        return MarketOrder::Status::Pending;
    }

    if (m_target_volume <= m_filled_volume) {
        return MarketOrder::Status::Filled;
    }

    return MarketOrder::Status::Rejected;
}
