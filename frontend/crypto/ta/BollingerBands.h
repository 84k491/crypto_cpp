#pragma once

#include "StandardDeviation.h"
#include "TimeWeightedMovingAverage.h"

#include <chrono>

struct BollingerBandsValue
{
    double m_upper_band;
    double m_trend;
    double m_lower_band;
};

class BollingerBands
{
public:
    BollingerBands(std::chrono::milliseconds interval, double std_deviation_coefficient);

    std::optional<BollingerBandsValue> push_value(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    double m_std_deviation_coefficient = 0.;

    StandardDeviation m_standard_deviation;
    TimeWeightedMovingAverage m_trend;
};
