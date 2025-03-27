#pragma once

#include "Candle.h"

#include <optional>
#include <queue>

class RelativeStrengthIndex
{
    class Average
    {
    public:
        Average(unsigned interval);

        std::optional<double> push(double value);

    private:
        unsigned m_interval = 0;

        double m_sum = 0.;
        std::queue<double> m_data;
    };

public:
    RelativeStrengthIndex(unsigned candles_interval);

    std::optional<double> push_candle(Candle c);

private:
    unsigned m_candles_interval = 0;

    Average m_positive;
    Average m_negative;
};
