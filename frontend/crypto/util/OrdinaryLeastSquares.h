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
    static constexpr double milliseconds_in_day = 24. * 60. * 60. * 1000.;

    PriceRegressionFunction(double k, double c)
        : k(k)
        , c(c)
    {
    }

    PriceRegressionFunction(SimpleRegressionFunction f, std::chrono::milliseconds zero_ts)
    {
        k = f.k / milliseconds_in_day;
        c = f.c - ((f.k * zero_ts.count()) / milliseconds_in_day);
    }

    double operator()(std::chrono::milliseconds x) const
    {
        return (k * x.count()) + c;
    }

    double k = 0.;
    double c = 0.;
};

SimpleRegressionFunction solve(const std::vector<Point> & data);
double deviation(const SimpleRegressionFunction & f, const std::vector<Point> & data);
std::vector<Point> from_prices(
        const std::vector<std::pair<std::chrono::milliseconds, double>> & prices);

std::pair<PriceRegressionFunction, double> solve_prices(
        const std::vector<std::pair<std::chrono::milliseconds, double>> & prices);

} // namespace OLS
