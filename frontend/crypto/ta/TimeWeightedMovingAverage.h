#pragma once

#include <chrono>
#include <list>
#include <optional>

class TimeWeightedMovingAverage
{
    struct Data
    {
        std::chrono::milliseconds timestamp;
        double value;
        double weight;
    };

public:
    TimeWeightedMovingAverage(std::chrono::milliseconds interval);

    // no data until the whole interval is filled
    std::optional<double> push_value(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    double calc_weight(size_t current);

private:
    std::list<Data> m_data;
    std::chrono::milliseconds m_interval;
    double m_sum = 0.;
    double m_total_weight = 0.;
};
