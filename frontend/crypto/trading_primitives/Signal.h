#pragma once

#include "Side.h"

#include <chrono>
#include <ostream>

struct Signal
{
    Side side;
    std::chrono::milliseconds timestamp;
    double price;
};
std::ostream & operator<<(std::ostream & os, const Signal & signal);
