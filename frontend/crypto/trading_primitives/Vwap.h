#pragma once

#include "Volume.h"

class Vwap
{
public:
    Vwap() = default;
    Vwap(const UnsignedVolume & vol, double price);

    auto value() const { return m_value; }

    Vwap & operator+=(const Vwap & other);
    Vwap & operator-=(const Vwap & other);

private:
    double m_value = {};
    double m_cumulative_volume = {};
};
