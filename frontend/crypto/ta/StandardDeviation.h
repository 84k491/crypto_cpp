#pragma once

#include <chrono>
#include <list>
#include <optional>

class StandardDeviation
{
public:
    StandardDeviation(std::chrono::milliseconds interval);

    std::optional<double> push_value(std::chrono::milliseconds ts, double value);

    double mean() const { return m_mean; }

private:
    struct Data
    {
        std::chrono::milliseconds ts;
        double value;
    };
    std::list<Data> m_values;
    std::chrono::milliseconds m_interval;

    double m_mean = 0.;
    double m_sum = 0.;
    double m_sq_sum = 0.;
};
