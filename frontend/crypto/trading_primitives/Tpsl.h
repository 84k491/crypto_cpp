#pragma once

#include <ostream>
struct Tpsl
{
    double take_profit_price = 0.;
    double stop_loss_price = 0.;
};

std::ostream & operator<<(std::ostream & os, const Tpsl & tpsl);
