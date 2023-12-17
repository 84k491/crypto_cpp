#pragma once

#include "Signal.h"
#include "SimpleMovingAverage.h"
#include "StandardDeviation.h"
#include <chrono>
#include <functional>

class BollingerBandsStrategy {
private:
    BollingerBandsStrategy(std::chrono::milliseconds interval, double std_deviation_coefficient);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

    void subscribe_for_strategy_internal(std::function<void(std::string name,
                                                            std::chrono::milliseconds ts,
                                                            double data)> && cb);

private:
    std::chrono::milliseconds m_interval;
    double m_std_deviation_coefficient = 0.;
    StandardDeviation m_standard_deviation;
    MovingAverage m_trend;

    std::vector<std::function<void(std::string name,
                                   std::chrono::milliseconds ts,
                                   double data)>>
            m_strategy_internal_callbacks;
};
