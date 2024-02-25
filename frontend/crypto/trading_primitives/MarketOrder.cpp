#include "MarketOrder.h"

std::ostream & operator<<(std::ostream & os, const MarketOrder & order)
{
    os << "MarketOrder: " << order.side_str() << "; volume: " << order.volume() << "; symbol: " << order.symbol();
    return os;
}

