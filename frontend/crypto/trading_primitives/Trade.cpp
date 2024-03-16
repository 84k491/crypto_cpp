#include "Trade.h"

// TODO move to Volume file
std::ostream & operator<<(std::ostream & os, const UnsignedVolume & volume)
{
    os << volume.value();
    return os;
}

std::ostream & operator<<(std::ostream & os, const Trade & trade)
{
    os << "\nTrade: {"
       << "\n  timestamp = " << trade.ts.count()
       << "\n  symbol = " << trade.symbol
       << "\n  price = " << trade.price
       << "\n  volume = " << trade.unsigned_volume
       << "\n  side = " << (trade.side == Side::Buy ? "Buy" : "Sell")
       << "\n  fee = " << trade.fee
       << "\n}";
    return os;
}
