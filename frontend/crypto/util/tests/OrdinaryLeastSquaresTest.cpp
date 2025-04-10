#include "OrdinaryLeastSquares.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

class OLSTest : public testing::Test
{
public:
    OLSTest() = default;
};

TEST_F(OLSTest, Trivial)
{
    std::vector<OLS::Point> data{
            {.x = 1., .y = 1.},
            {.x = 2., .y = 2.},
            {.x = 3., .y = 3.},
            {.x = 4., .y = 4.},
            {.x = 5., .y = 5.}};

    const auto f = OLS::solve(data);
    EXPECT_DOUBLE_EQ(f.k, 1.);
    EXPECT_DOUBLE_EQ(f.c, 0);

    double dev = OLS::deviation(f, data);
    EXPECT_DOUBLE_EQ(dev, 0.);

    EXPECT_DOUBLE_EQ(f(15.6), 15.6);
}

TEST_F(OLSTest, General)
{
    std::vector<OLS::Point> data{
            {.x = 1., .y = 2.},
            {.x = 2., .y = 2.},
            {.x = 3., .y = 3.},
            {.x = 4., .y = 3.},
            {.x = 5., .y = 5.},
            {.x = 6., .y = 4.},
            {.x = 7., .y = 5.},
            {.x = 8., .y = 8.},
            {.x = 9., .y = 7.},
            {.x = 10., .y = 7.}};

    const auto f = OLS::solve(data);
    EXPECT_DOUBLE_EQ(f.k, 0.66666666666666663);
    EXPECT_DOUBLE_EQ(f.c, 0.93333333333333357);

    double dev = OLS::deviation(f, data);
    EXPECT_DOUBLE_EQ(dev, 0.75718777944003646);

    EXPECT_DOUBLE_EQ(f(20), 14.266666666666666);
}

TEST_F(OLSTest, Converters)
{
    std::vector<std::pair<std::chrono::milliseconds, double>> data{
            {std::chrono::milliseconds{1743796573'222}, 2},
            {std::chrono::milliseconds{1743796574'333}, 3},
            {std::chrono::milliseconds{1743796575'444}, 4},
            {std::chrono::milliseconds{1743796576'555}, 5},
            {std::chrono::milliseconds{1743796577'666}, 6},
            {std::chrono::milliseconds{1743796578'777}, 7},
            {std::chrono::milliseconds{1743796579'888}, 8},
    };
    const auto expected_zero = std::pair{std::chrono::milliseconds{1743796573'222}, 2};
    const auto expected_fut = std::pair{std::chrono::milliseconds{1743796580'999}, 9};

    const auto points = OLS::from_prices(data);
    const auto f = OLS::solve(points);
    const auto cf = OLS::PriceRegressionFunction(f, data.front().first);
    EXPECT_DOUBLE_EQ(cf(expected_zero.first), expected_zero.second);
    EXPECT_DOUBLE_EQ(cf(expected_fut.first), expected_fut.second);
}
