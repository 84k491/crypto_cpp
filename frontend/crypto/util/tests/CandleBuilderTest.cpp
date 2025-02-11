#include "CandleBuiler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

class CandleBuilderTest : public testing::Test
{
public:
    CandleBuilderTest() = default;
};

TEST_F(CandleBuilderTest, NoCandleOnFirstTrade)
{
    constexpr std::chrono::milliseconds timeframe{std::chrono::hours{1}};
    CandleBuilder cb{timeframe};

    std::vector<Candle> candles = cb.push_trade(1., SignedVolume{1.}, std::chrono::milliseconds{1});
    EXPECT_EQ(candles.size(), 0);
}

// 1739077200 - Sun Feb 09 2025 05:00:00 GMT+0000
TEST_F(CandleBuilderTest, BuildCandleBasic)
{
    constexpr std::chrono::milliseconds timeframe{std::chrono::hours{1}};
    CandleBuilder cb{timeframe};

    const auto open_ts = std::chrono::milliseconds{1739077200};
    auto current_time = open_ts;

    current_time += std::chrono::milliseconds{2};
    const double open_price = 10.;
    EXPECT_EQ(cb.push_trade(open_price, SignedVolume{1.}, current_time).size(), 0);

    current_time += std::chrono::milliseconds{3};
    const double low_price = 5.;
    EXPECT_EQ(cb.push_trade(low_price, SignedVolume{1.}, current_time).size(), 0);

    current_time += std::chrono::milliseconds{1};
    const double high_price = 15.;
    EXPECT_EQ(cb.push_trade(high_price, SignedVolume{1.}, current_time).size(), 0);

    current_time += std::chrono::milliseconds{10};
    const double close_price = 9.;
    EXPECT_EQ(cb.push_trade(close_price, SignedVolume{1.}, current_time).size(), 0);

    const auto next_candle_ts = open_ts + timeframe + std::chrono::milliseconds{1};
    const auto candles = cb.push_trade(1., SignedVolume{1.}, next_candle_ts);
    EXPECT_EQ(candles.size(), 1);
    EXPECT_EQ(candles[0].open(), open_price);
    EXPECT_EQ(candles[0].high(), high_price);
    EXPECT_EQ(candles[0].low(), low_price);
    EXPECT_EQ(candles[0].close(), close_price);
    EXPECT_EQ(candles[0].volume(), 4.); // total volume
}

TEST_F(CandleBuilderTest, TwoEmptyCandlesOnBigGap)
{
    constexpr std::chrono::milliseconds timeframe{std::chrono::hours{1}};
    CandleBuilder cb{timeframe};

    const auto open_ts = std::chrono::milliseconds{1739077200};
    auto current_time = open_ts;

    current_time += std::chrono::milliseconds{2};
    const double open_price = 10.;
    EXPECT_EQ(cb.push_trade(open_price, SignedVolume{1.}, current_time).size(), 0);

    current_time += std::chrono::milliseconds{3};
    const double low_price = 5.;
    EXPECT_EQ(cb.push_trade(low_price, SignedVolume{1.}, current_time).size(), 0);

    current_time += std::chrono::milliseconds{1};
    const double high_price = 15.;
    EXPECT_EQ(cb.push_trade(high_price, SignedVolume{1.}, current_time).size(), 0);

    current_time += std::chrono::milliseconds{10};
    const double close_price = 9.;
    EXPECT_EQ(cb.push_trade(close_price, SignedVolume{1.}, current_time).size(), 0);

    // skipping two next candles
    const auto next_candle_ts = open_ts + 3 * timeframe + std::chrono::milliseconds{1};
    const auto candles = cb.push_trade(1., SignedVolume{1.}, next_candle_ts);
    EXPECT_EQ(candles.size(), 3);
    EXPECT_EQ(candles[2].open(), open_price);
    EXPECT_EQ(candles[2].high(), high_price);
    EXPECT_EQ(candles[2].low(), low_price);
    EXPECT_EQ(candles[2].close(), close_price);
    EXPECT_EQ(candles[2].volume(), 4.); // total volume

    EXPECT_EQ(candles[1].open(), close_price);
    EXPECT_EQ(candles[1].high(), close_price);
    EXPECT_EQ(candles[1].low(), close_price);
    EXPECT_EQ(candles[1].close(), close_price);
    EXPECT_EQ(candles[1].volume(), 0.);
}
