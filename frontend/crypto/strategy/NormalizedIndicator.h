#pragma once

#include "StrategyInterface.h"

#include <string>

/*
---------------- 2.0
long signal
---------------- 1.0 upper threshold
no signal
---------------- 0.0
no signal
---------------- -1.0 lower threshold
short signal
---------------- -2.0
*/
class NormalizedIndicator
{
public:
    NormalizedIndicator(std::string chart_name, std::string series_name, bool inverted);

    [[nodiscard]]
    StrategyInternalData push(double upper_threshold, double value, double lower_threshold) const;

private:
    std::string m_chart_name;
    std::string m_series_name;
    bool m_inverted = false;
};
