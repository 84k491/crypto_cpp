#include "PositionManager.h"

#include "Trade.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace test {

using namespace testing;
constexpr double double_epsilon = 0.00001;

class PositionManagerTest : public Test
{
public:
    PositionManagerTest()
        : m_symbol("BTCUSD")
    {
    }

protected:
    Symbol m_symbol;
};

// open a long position, check parameters
TEST_F(PositionManagerTest, OpenLongDontClose)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(1234567),
                     m_symbol.symbol_name,
                     {},
                     1000.,
                     UnsignedVolume::from(10.).value(),
                     Side::buy(),
                     0.2);

    const auto res_opt = pm.on_trade_received(open_trade);
    EXPECT_FALSE(res_opt.has_value());
    const auto * opened_pos_ptr = pm.opened();
    ASSERT_TRUE(opened_pos_ptr != nullptr);
    const auto & opened_pos = *opened_pos_ptr;

    EXPECT_EQ(opened_pos.side(), Side::buy());
    EXPECT_EQ(opened_pos.opened_volume().value(), 10.);
    EXPECT_EQ(opened_pos.open_ts().count(), 1234567);
    EXPECT_EQ(opened_pos.total_entry_fee(), 0.2);
}

// open a short position, check parameters
TEST_F(PositionManagerTest, OpenShortDontClose)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(1234567),
                     m_symbol.symbol_name,
                     {},
                     1000.,
                     UnsignedVolume::from(10.).value(),
                     Side::sell(),
                     0.2);

    const auto res_opt = pm.on_trade_received(open_trade);
    EXPECT_FALSE(res_opt.has_value());
    const auto * opened_pos_ptr = pm.opened();
    ASSERT_TRUE(opened_pos_ptr != nullptr);
    const auto & opened_pos = *opened_pos_ptr;

    EXPECT_EQ(opened_pos.side(), Side::sell());
    EXPECT_EQ(opened_pos.opened_volume().value(), -10.);
    EXPECT_EQ(opened_pos.open_ts().count(), 1234567);
    EXPECT_EQ(opened_pos.total_entry_fee(), 0.2);
}

// open a long position, close it
// profit is bigger than fee, fees subtracted from profit
// fees calculated
TEST_F(PositionManagerTest, LongCloseRawProfitBiggerThanFee)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     1000.,
                     UnsignedVolume::from(10.).value(),
                     Side::buy(),
                     0.2);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      1500.,
                      UnsignedVolume::from(10.).value(),
                      Side::sell(),
                      0.3);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    EXPECT_EQ(res.pnl_with_fee, 5000 - 0.2 - 0.3);
    EXPECT_EQ(res.fees_paid, 0.2 + 0.3);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open a short position, close it
// profit is bigger than fee, fees subtracted from profit
// fees calculated
TEST_F(PositionManagerTest, ShortCloseRawProfitBiggerThanFee)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     1500.,
                     UnsignedVolume::from(10.).value(),
                     Side::sell(),
                     0.2);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      1000.,
                      UnsignedVolume::from(10.).value(),
                      Side::buy(),
                      0.3);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    EXPECT_EQ(res.pnl_with_fee, 5000 - 0.2 - 0.3);
    EXPECT_EQ(res.fees_paid, 0.2 + 0.3);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open a long position, close it
// raw profit is less than fee, fees subtracted from profit
// total profit is negative
// fees calculated
TEST_F(PositionManagerTest, LongCloseRawProfitLessThanFee)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     10.,
                     UnsignedVolume::from(1.).value(),
                     Side::buy(),
                     0.2);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      10.4,
                      UnsignedVolume::from(1.).value(),
                      Side::sell(),
                      0.3);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    EXPECT_NEAR(res.pnl_with_fee, 0.4 - 0.2 - 0.3, double_epsilon);
    EXPECT_EQ(res.fees_paid, 0.2 + 0.3);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open a short position, close it
// raw profit is less than fee, fees subtracted from profit
// total profit is negative
// fees calculated
TEST_F(PositionManagerTest, ShortCloseRawProfitLessThanFee)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     10,
                     UnsignedVolume::from(1.).value(),
                     Side::sell(),
                     0.2);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      9.6,
                      UnsignedVolume::from(1.).value(),
                      Side::buy(),
                      0.3);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    EXPECT_NEAR(res.pnl_with_fee, 0.4 - 0.2 - 0.3, double_epsilon);
    EXPECT_EQ(res.fees_paid, 0.2 + 0.3);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open a long position, close it
// same price, fees subtracted from profit
// fees calculated
// total profit is negative and the same as fees * -1
TEST_F(PositionManagerTest, LongCloseSamePrice)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     10.,
                     UnsignedVolume::from(1.).value(),
                     Side::buy(),
                     0.2);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      10.,
                      UnsignedVolume::from(1.).value(),
                      Side::sell(),
                      0.3);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    EXPECT_NEAR(res.pnl_with_fee, -0.2 - 0.3, double_epsilon);
    EXPECT_EQ(res.fees_paid, 0.2 + 0.3);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open a short position, close it
// same price, fees subtracted from profit
// fees calculated
// total profit is negative and the same as fees * -1
TEST_F(PositionManagerTest, ShortCloseSamePrice)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     10.,
                     UnsignedVolume::from(1.).value(),
                     Side::sell(),
                     0.2);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      10.,
                      UnsignedVolume::from(1.).value(),
                      Side::buy(),
                      0.3);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    EXPECT_NEAR(res.pnl_with_fee, -0.2 - 0.3, double_epsilon);
    EXPECT_EQ(res.fees_paid, 0.2 + 0.3);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open a long position with two trades, close it with one
// position value calculated from the average price
TEST_F(PositionManagerTest, LongOpenedWithTwoTradesProfit)
{
    PositionManager pm(m_symbol);

    Trade open_trade1{std::chrono::milliseconds(123),
                      m_symbol.symbol_name,
                      {},
                      10.,
                      UnsignedVolume::from(1.).value(),
                      Side::buy(),
                      0.2};
    Trade open_trade2{std::chrono::milliseconds(133),
                      m_symbol.symbol_name,
                      {},
                      12.,
                      UnsignedVolume::from(1.).value(),
                      Side::buy(),
                      0.3};
    pm.on_trade_received(open_trade1);
    pm.on_trade_received(open_trade2);

    ASSERT_TRUE(pm.opened());
    const auto & pos = *pm.opened();
    EXPECT_EQ(pos.side(), Side::buy());
    EXPECT_EQ(pos.open_ts().count(), 123);
    EXPECT_EQ(pos.total_entry_fee(), 0.5);
    EXPECT_EQ(pos.opened_volume().value(), 2.);
    EXPECT_EQ(pos.avg_entry_price(), 11.);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      20.,
                      UnsignedVolume::from(2.).value(),
                      Side::sell(),
                      0.4);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    constexpr double avg_open_price = (10. + 12.) / 2.;
    constexpr double expected_raw_profit = (20. - avg_open_price) * /*vol*/ 2.;
    const double fee_sum = 0.2 + 0.3 + 0.4;
    EXPECT_NEAR(res.pnl_with_fee, expected_raw_profit - fee_sum, double_epsilon);
    EXPECT_EQ(res.fees_paid, fee_sum);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// same as above, but for short
TEST_F(PositionManagerTest, ShortOpenedWithTwoTradesProfit)
{
    PositionManager pm(m_symbol);

    Trade open_trade1{std::chrono::milliseconds(123),
                      m_symbol.symbol_name,
                      {},
                      10.,
                      UnsignedVolume::from(1.).value(),
                      Side::sell(),
                      0.2};
    Trade open_trade2{std::chrono::milliseconds(133),
                      m_symbol.symbol_name,
                      {},
                      8.,
                      UnsignedVolume::from(1.).value(),
                      Side::sell(),
                      0.3};
    pm.on_trade_received(open_trade1);
    pm.on_trade_received(open_trade2);

    ASSERT_TRUE(pm.opened());
    const auto & pos = *pm.opened();
    EXPECT_EQ(pos.side(), Side::sell());
    EXPECT_EQ(pos.open_ts().count(), 123);
    EXPECT_EQ(pos.total_entry_fee(), 0.5);
    EXPECT_EQ(pos.opened_volume().value(), -2.);
    EXPECT_EQ(pos.avg_entry_price(), 9.);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      6.,
                      UnsignedVolume::from(2.).value(),
                      Side::buy(),
                      0.4);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    constexpr double avg_open_price = (10. + 8.) / 2.;
    constexpr double expected_raw_profit = (avg_open_price - 6.) * /*vol*/ 2.;
    const double fee_sum = 0.2 + 0.3 + 0.4;
    EXPECT_NEAR(res.pnl_with_fee, expected_raw_profit - fee_sum, double_epsilon);
    EXPECT_EQ(res.fees_paid, fee_sum);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open long with one trade, close with two
// position value calculated from the average price
TEST_F(PositionManagerTest, LongClosedWithTwoTradesProfit)
{
    PositionManager pm(m_symbol);

    Trade open_trade{std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     10.,
                     UnsignedVolume::from(2.).value(),
                     Side::buy(),
                     0.2};
    pm.on_trade_received(open_trade);

    Trade close_trade1(std::chrono::milliseconds(213),
                       m_symbol.symbol_name,
                       {},
                       12.,
                       UnsignedVolume::from(1.).value(),
                       Side::sell(),
                       0.3);
    EXPECT_TRUE(pm.on_trade_received(close_trade1).has_value());

    ASSERT_TRUE(pm.opened());
    const auto & pos = *pm.opened();
    EXPECT_EQ(pos.side(), Side::buy());
    EXPECT_EQ(pos.total_entry_fee(), 0.1) << "Half of volume traded -> half fee extracted";
    EXPECT_EQ(pos.opened_volume().value(), 1.);
    EXPECT_EQ(pos.avg_entry_price(), 10.);

    Trade close_trade2(std::chrono::milliseconds(223),
                       m_symbol.symbol_name,
                       {},
                       14.,
                       UnsignedVolume::from(1.).value(),
                       Side::sell(),
                       0.4);
    const auto res_opt = pm.on_trade_received(close_trade2);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    constexpr double avg_close_price = (12 + 14) / 2.;
    constexpr double expected_raw_profit = (avg_close_price - 10.) * /*vol*/ 2.;
    const double fee_sum = 0.2 + 0.3 + 0.4;
    EXPECT_NEAR(res.pnl_with_fee, expected_raw_profit - fee_sum, double_epsilon);
    EXPECT_NEAR(res.fees_paid, fee_sum, double_epsilon);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// same as above, but for short
TEST_F(PositionManagerTest, ShortClosedWithTwoTradesProfit)
{
    PositionManager pm(m_symbol);

    Trade open_trade{std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     10.,
                     UnsignedVolume::from(2.).value(),
                     Side::sell(),
                     0.2};
    pm.on_trade_received(open_trade);

    Trade close_trade1(std::chrono::milliseconds(213),
                       m_symbol.symbol_name,
                       {},
                       8.,
                       UnsignedVolume::from(1.).value(),
                       Side::buy(),
                       0.3);
    Trade close_trade2(std::chrono::milliseconds(223),
                       m_symbol.symbol_name,
                       {},
                       6.,
                       UnsignedVolume::from(1.).value(),
                       Side::buy(),
                       0.4);
    EXPECT_TRUE(pm.on_trade_received(close_trade1).has_value());
    const auto res_opt = pm.on_trade_received(close_trade2);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    constexpr double avg_close_price = (6. + 8.) / 2.;
    constexpr double expected_raw_profit = (10. - avg_close_price) * /*vol*/ 2.;
    const double fee_sum = 0.2 + 0.3 + 0.4;
    EXPECT_NEAR(res.pnl_with_fee, expected_raw_profit - fee_sum, double_epsilon);
    EXPECT_NEAR(res.fees_paid, fee_sum, double_epsilon);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open long, flip to short, close
TEST_F(PositionManagerTest, LongFlipClosedWithProfit)
{
    PositionManager pm(m_symbol);

    Trade open_trade{std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     10.,
                     UnsignedVolume::from(1.).value(),
                     Side::buy(),
                     0.2};
    pm.on_trade_received(open_trade);

    Trade flip_trade(std::chrono::milliseconds(213),
                     m_symbol.symbol_name,
                     {},
                     12.,
                     UnsignedVolume::from(2.).value(),
                     Side::sell(),
                     0.3);
    EXPECT_TRUE(pm.on_trade_received(flip_trade).has_value());

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      8.,
                      UnsignedVolume::from(1.).value(),
                      Side::buy(),
                      0.4);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    constexpr double expected_raw_profit = 2 + 4;
    const double fee_sum = 0.2 + 0.3 + 0.4;
    EXPECT_NEAR(res.pnl_with_fee, expected_raw_profit - fee_sum, double_epsilon);
    EXPECT_NEAR(res.fees_paid, fee_sum, double_epsilon);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// same for short
TEST_F(PositionManagerTest, ShortFlipClosedWithProfit)
{
    PositionManager pm(m_symbol);

    Trade open_trade{std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     12.,
                     UnsignedVolume::from(1.).value(),
                     Side::sell(),
                     0.2};
    pm.on_trade_received(open_trade);

    Trade flip_trade(std::chrono::milliseconds(213),
                     m_symbol.symbol_name,
                     {},
                     10.,
                     UnsignedVolume::from(2.).value(),
                     Side::buy(),
                     0.3);
    EXPECT_TRUE(pm.on_trade_received(flip_trade).has_value());

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      14.,
                      UnsignedVolume::from(1.).value(),
                      Side::sell(),
                      0.4);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    constexpr double expected_raw_profit = 2 + 4;
    const double fee_sum = 0.2 + 0.3 + 0.4;
    EXPECT_NEAR(res.pnl_with_fee, expected_raw_profit - fee_sum, double_epsilon);
    EXPECT_NEAR(res.fees_paid, fee_sum, double_epsilon);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// open long, close with loss, fees subtracted, total loss is increased
TEST_F(PositionManagerTest, LongCloseWithLoss)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     1000.,
                     UnsignedVolume::from(10.).value(),
                     Side::buy(),
                     0.2);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      900.,
                      UnsignedVolume::from(10.).value(),
                      Side::sell(),
                      0.3);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    EXPECT_EQ(res.pnl_with_fee, -1000 - 0.2 - 0.3);
    EXPECT_EQ(res.fees_paid, 0.2 + 0.3);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// same for short
TEST_F(PositionManagerTest, ShortCloseWithLoss)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     900.,
                     UnsignedVolume::from(10.).value(),
                     Side::sell(),
                     0.2);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      1000.,
                      UnsignedVolume::from(10.).value(),
                      Side::buy(),
                      0.3);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    EXPECT_EQ(res.pnl_with_fee, -1000 - 0.2 - 0.3);
    EXPECT_EQ(res.fees_paid, 0.2 + 0.3);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// same as above, but with fractional price
TEST_F(PositionManagerTest, LongCloseWithLossFractionalPrice)
{
    PositionManager pm(m_symbol);

    Trade open_trade(std::chrono::milliseconds(123),
                     m_symbol.symbol_name,
                     {},
                     0.05802,
                     UnsignedVolume::from(1723).value(),
                     Side::buy(),
                     0.0549827);
    pm.on_trade_received(open_trade);

    Trade close_trade(std::chrono::milliseconds(223),
                      m_symbol.symbol_name,
                      {},
                      0.05796,
                      UnsignedVolume::from(1723).value(),
                      Side::sell(),
                      0.0549258);
    const auto res_opt = pm.on_trade_received(close_trade);
    ASSERT_TRUE(res_opt.has_value());
    EXPECT_FALSE(pm.opened());
    const auto & res = res_opt.value();

    const auto open_amount = 0.05802 * 1723;
    const auto close_amount = 0.05796 * 1723;
    const auto fees = 0.0549827 + 0.0549258;
    EXPECT_NEAR(res.pnl_with_fee, close_amount - open_amount - fees, double_epsilon);
    EXPECT_EQ(res.fees_paid, fees);
    EXPECT_EQ(res.opened_time().count(), 100);
}

// two positions in a row, first long, then short
TEST_F(PositionManagerTest, ShortProfitThenLongLoss)
{
    PositionManager pm(m_symbol);

    // Short with profit
    {
        Trade open_trade(std::chrono::milliseconds(123),
                         m_symbol.symbol_name,
                         {},
                         0.0584,
                         UnsignedVolume::from(1712).value(),
                         Side::sell(),
                         0.0549894);
        pm.on_trade_received(open_trade);

        Trade close_trade(std::chrono::milliseconds(223),
                          m_symbol.symbol_name,
                          {},
                          0.0582,
                          UnsignedVolume::from(1712).value(),
                          Side::buy(),
                          0.0548011);
        const auto res_opt = pm.on_trade_received(close_trade);
        ASSERT_TRUE(res_opt.has_value());
        EXPECT_FALSE(pm.opened());
        const auto & res = res_opt.value();

        const auto open_amount = 0.0584 * 1712;
        const auto close_amount = 0.0582 * 1712;
        const auto fees = 0.0549894 + 0.0548011;
        EXPECT_NEAR(res.pnl_with_fee, open_amount - close_amount - fees, double_epsilon);
        EXPECT_EQ(res.fees_paid, fees);
        EXPECT_EQ(res.opened_time().count(), 100);
    }

    // Long with loss
    {
        Trade open_trade(std::chrono::milliseconds(323),
                         m_symbol.symbol_name,
                         {},
                         0.05802,
                         UnsignedVolume::from(1723).value(),
                         Side::buy(),
                         0.0549827);
        pm.on_trade_received(open_trade);

        Trade close_trade(std::chrono::milliseconds(423),
                          m_symbol.symbol_name,
                          {},
                          0.05796,
                          UnsignedVolume::from(1723).value(),
                          Side::sell(),
                          0.0549258);
        const auto res_opt = pm.on_trade_received(close_trade);
        ASSERT_TRUE(res_opt.has_value());
        EXPECT_FALSE(pm.opened());
        const auto & res = res_opt.value();

        const auto open_amount = 0.05802 * 1723;
        const auto close_amount = 0.05796 * 1723;
        const auto fees = 0.0549827 + 0.0549258;
        EXPECT_NEAR(res.pnl_with_fee,
                    close_amount - open_amount - fees,
                    double_epsilon);
        EXPECT_EQ(res.fees_paid, fees);
        EXPECT_EQ(res.opened_time().count(), 100);
    }
}

// TODO
// TEST_F(PositionManagerTest, PriceLevels)

} // namespace test
