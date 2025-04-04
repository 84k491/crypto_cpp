#include "OrdinaryLeastSquares.h"

#include <cmath>

namespace OLS {

SimpleRegressionFunction solve(const std::vector<Point> & d)
{
    const unsigned n = d.size();

    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;

    for (unsigned i = 0; i < n; i++) {
        sum_x += d[i].x;
        sum_y += d[i].y;
        sum_xy += d[i].x * d[i].y;
        sum_xx += d[i].x * d[i].x;
    }

    const double k = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
    const double c = (sum_y - k * sum_x) / n;
    return {.k = k, .c = c};
}

double deviation(const SimpleRegressionFunction & f, const std::vector<Point> & data)
{
    if (data.size() < 2) {
        return 0.;
    }

    double dev = 0.;
    for (const auto & p : data) {
        const auto y_calc = f(p.x);
        const auto err = p.y - y_calc;
        dev += err * err;
    }
    return sqrt(dev) / ((unsigned)data.size() - 1);
}

std::vector<Point> from_prices(std::vector<std::pair<std::chrono::milliseconds, double>> & prices)
{
    // must be in ascending order

    std::vector<Point> res;
    res.reserve(prices.size());

    constexpr unsigned milliseconds_in_day = 24 * 60 * 60 * 1000;
    const auto first_ts = prices.front().first.count();

    for (const auto & p : prices) {
        const auto days_from_begin = double(p.first.count() - first_ts) / milliseconds_in_day;
        res.push_back({.x = days_from_begin, .y = p.second});
    }
    return res;
}

std::pair<PriceRegressionFunction, double> solve_prices(
        std::vector<std::pair<std::chrono::milliseconds, double>> & prices)
{
    const auto points = from_prices(prices);
    const auto f = solve(points);
    const double dev = deviation(f, points);
    const auto reg = PriceRegressionFunction{f, prices.front().first};
    return {reg, dev};
}

} // namespace OLS
