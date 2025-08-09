#include "GridLevels.h"

#include <gtest/gtest.h>

namespace test {

class GridLevelsTest : public testing::Test
{
};

TEST_F(GridLevelsTest, GetPositiveLevels)
{
    using namespace GridLevels;

    double trend = 6;
    double width = 0.2;

    {
        double price = 6.1;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, 0);
    }

    {
        double price = 6.19;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, 0);
    }

    {
        double price = 6.20;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, 1);
    }

    {
        double price = 6.39999;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, 1);
    }

    {
        double price = 6.4;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, 2);
    }
}

TEST_F(GridLevelsTest, GetNegativeLevels)
{
    using namespace GridLevels;

    double trend = 6;
    double width = 0.2;

    {
        double price = 5.9;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, 0);
    }

    {
        double price = 5.81;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, 0);
    }

    {
        double price = 5.8;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, -1);
    }

    {
        double price = 5.7;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, -1);
    }

    {
        double price = 5.6000001;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, -1);
    }

    {
        double price = 5.6;
        const int level = get_level_number(price, trend, width);
        EXPECT_EQ(level, -2);
    }
}

TEST_F(GridLevelsTest, GetPositivePrices)
{
    using namespace GridLevels;

    double trend = 6;
    double width = 0.2;

    {
        int level_num = 0;
        double price = get_price_from_level_number(level_num, trend, width);
        EXPECT_DOUBLE_EQ(price, trend);
    }

    {
        int level_num = 1;
        double price = get_price_from_level_number(level_num, trend, width);
        EXPECT_DOUBLE_EQ(price, 6.2);
    }

    {
        int level_num = 2;
        double price = get_price_from_level_number(level_num, trend, width);
        EXPECT_DOUBLE_EQ(price, 6.4);
    }
}

TEST_F(GridLevelsTest, GetNegativePrices)
{
    using namespace GridLevels;

    double trend = 6;
    double width = 0.2;

    {
        int level_num = -1;
        double price = get_price_from_level_number(level_num, trend, width);
        EXPECT_DOUBLE_EQ(price, 5.8);
    }

    {
        int level_num = -2;
        double price = get_price_from_level_number(level_num, trend, width);
        EXPECT_DOUBLE_EQ(price, 5.6);
    }
}
} // namespace test
