#include "Trade.h"

std::ostream & operator<<(std::ostream & os, const Trade & trade)
{
    os << "\nTrade: {"
       << "\n  timestamp = " << trade.m_ts.count()
       << "\n  symbol = " << trade.m_symbol
       << "\n  price = " << trade.m_price
       << "\n  volume = " << trade.m_unsigned_volume
       << "\n  side = " << (trade.m_side == Side::Buy ? "Buy" : "Sell")
       << "\n  fee = " << trade.m_fee
       << "\n}";
    return os;
}
