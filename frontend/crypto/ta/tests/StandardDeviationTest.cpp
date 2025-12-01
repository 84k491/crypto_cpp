#include "StandardDeviation.h"

#include <gtest/gtest.h>

class StandardDeviationTest : public testing::Test
{
public:
};

struct TestValues
{
    int sec;
    double input;
    double expected;
};

using namespace std::chrono;

TEST_F(StandardDeviationTest, Positive)
{
    StandardDeviation sd{seconds{5}};

    std::vector<TestValues> values = {
            {1, 0.1, 0},
            {2, 0.2, 0.05},
            {3, 0.3, 0.0816497},
            {4, 0.4, 0.111803},
            {5, 0.5, 0.141421},
            {6, 0.6, 0.170783},
            {7, 0.7, 0.170783},
            {8, 0.8, 0.170783},
            {9, 1.0, 0.197203},
    };

    for (const auto [sec, v, expected] : values) {
        auto res_opt = sd.push_value(seconds{sec}, v);
        if (!res_opt) {
            continue;
        }

        EXPECT_NEAR(expected, *res_opt, 1e-6);
    }
}

TEST_F(StandardDeviationTest, Negative)
{
    StandardDeviation sd{seconds{5}};

    std::vector<TestValues> values = {
            {1, -0.1, 0},
            {2, -0.2, 0.05},
            {3, -0.3, 0.0816497},
            {4, -0.4, 0.111803},
            {5, -0.5, 0.141421},
            {6, -0.6, 0.170783},
            {7, -0.7, 0.170783},
            {8, -0.8, 0.170783},
            {9, -1.0, 0.197203},
    };

    for (const auto [sec, v, expected] : values) {
        auto res_opt = sd.push_value(seconds{sec}, v);
        if (!res_opt) {
            continue;
        }

        EXPECT_NEAR(expected, *res_opt, 1e-6);
    }
}

TEST_F(StandardDeviationTest, NegativeAndPositive)
{
    StandardDeviation sd{seconds{5}};

    std::vector<TestValues> values = {
            {1, -0.1, 0},
            {2, +0.2, 0.15},
            {3, -0.3, 0.20548},
            {4, +0.4, 0.269258},
            {5, -0.5, 0.32619},
            {6, +0.6, 0.386221},
            {7, -0.7, 0.478714},
            {8, +0.8, 0.57373},
            {9, -1.0, 0.692018},
    };

    for (const auto [sec, v, expected] : values) {
        auto res_opt = sd.push_value(seconds{sec}, v);
        if (!res_opt) {
            continue;
        }

        EXPECT_NEAR(expected, *res_opt, 1e-6);
    }
}
