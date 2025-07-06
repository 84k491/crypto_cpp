#include "Trade.h"

std::ostream & operator<<(std::ostream & os, const Trade & trade)
{
    os << "\nTrade: {"
       << "\n  timestamp = " << trade.m_ts.count()
       << "\n  order_guid = " << trade.m_order_guid
       << "\n  symbol_name = " << trade.m_symbol_name
       << "\n  price = " << trade.m_price
       << "\n  volume = " << trade.m_unsigned_volume
       << "\n  side = " << trade.m_side
       << "\n  fee = " << trade.m_fee
       << "\n}";
    return os;
}
