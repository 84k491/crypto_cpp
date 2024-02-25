#pragma once

#include "Enums.h"

#include <chrono>
#include <ostream>

struct Signal
{
    Side side;
    std::chrono::milliseconds timestamp;
    double price;
};
std::ostream & operator<<(std::ostream & os, const Signal & signal);
