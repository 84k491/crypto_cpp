#pragma once

#include <chrono>
#include <utility>
#include <vector>

namespace OLS {

struct SimpleRegressionFunction
{
    double operator()(double x) const { return (k * x) + c; }

    double k = 0.;
    double c = 0.;
};

struct Point
{
    double x;
    double y;
};

struct PriceRegressionFunction
{
    PriceRegressionFunction(SimpleRegressionFunction f, std::chrono::milliseconds zero_ts)
        : f(f)
        , zero_ts(zero_ts)
    {
    }

    static constexpr unsigned milliseconds_in_day = 24 * 60 * 60 * 1000;

    double operator()(std::chrono::milliseconds x) const
    {
        const auto days_from_begin = double(x.count() - zero_ts.count()) / milliseconds_in_day;
        return f(days_from_begin);
    }

    SimpleRegressionFunction f;
    std::chrono::milliseconds zero_ts;
};

SimpleRegressionFunction solve(const std::vector<Point> & data);
double deviation(const SimpleRegressionFunction & f, const std::vector<Point> & data);
std::vector<Point> from_prices(
        const std::vector<std::pair<std::chrono::milliseconds, double>> & prices);

std::pair<PriceRegressionFunction, double> solve_prices(
        const std::vector<std::pair<std::chrono::milliseconds, double>> & prices);

} // namespace OLS
