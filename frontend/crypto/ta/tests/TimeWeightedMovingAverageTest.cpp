#include "TimeWeightedMovingAverage.h"

#include <gtest/gtest.h>

class TimeWeightedMovingAverageTest : public testing::Test
{
public:
};

TEST_F(TimeWeightedMovingAverageTest, NoValueUntilIntervalFilled)
{
    TimeWeightedMovingAverage m_twma(std::chrono::milliseconds(10));

    // ts, price, expected
    const auto test_data = std::vector<std::tuple<size_t, double>>{
            {1, 1.0},
            {2, 2.0},
            {3, 3.0},
            {9, 9.0},
            {10, 10.0},
            {10, 11.0},
    };

    for (const auto & [ts, price] : test_data) {
        const auto output_opt = m_twma.push_value(std::make_pair(std::chrono::milliseconds(ts), price));
        ASSERT_FALSE(output_opt.has_value()) << "ts: " << ts << ", price: " << price;
    }

    const auto output_opt = m_twma.push_value(std::make_pair(std::chrono::milliseconds(11), 11));
    ASSERT_TRUE(output_opt.has_value());
}

TEST_F(TimeWeightedMovingAverageTest, Calculation)
{
    TimeWeightedMovingAverage m_twma(std::chrono::milliseconds(10));

    // ts, price, expected
    const auto test_data = std::vector<std::tuple<size_t, double, std::optional<double>>>{
            {1, 1.0, std::nullopt},
            {2, 2.0, std::nullopt},
            {3, 3.0, std::nullopt},
            {9, 9.0, std::nullopt},
            {10, 10.0, std::nullopt},
            {11, 11.0, 4},
            {15, 15.0, 6.384615384615385},
            {35, 35.0, 11.90625},
    };
    // 1 + 2 + 3*6 + 9 + 10

    for (const auto & [ts, price, expected] : test_data) {
        std::stringstream ss;
        ss << "ts: " << ts << ", price: " << price << ", expected: " << (expected.has_value() ? std::to_string(expected.value()) : "nullopt");
        const auto output_opt = m_twma.push_value(std::make_pair(std::chrono::milliseconds(ts), price));
        if (!expected.has_value()) {
            ASSERT_FALSE(output_opt.has_value()) << ss.str();
            continue;
        }

        ASSERT_TRUE(output_opt.has_value()) << ss.str();
        ASSERT_DOUBLE_EQ(output_opt.value(), expected.value()) << ss.str();
    }
}
