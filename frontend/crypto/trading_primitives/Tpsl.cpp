#include "Tpsl.h"

std::ostream & operator<<(std::ostream & os, const Tpsl & tpsl)
{
    os << "\nTpsl: {"
       << "\n  take_profit_price = " << tpsl.take_profit_price
       << "\n  stop_loss_price = " << tpsl.stop_loss_price
       << "\n}";
    return os;
}
