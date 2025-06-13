#include "BacktestTradingGateway.h"

#include "EventLoop.h"
#include "EventTimeseriesChannel.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

namespace test {
using namespace testing;

class MockEventLoop : public ILambdaAcceptor
{
    bool push_to_queue(LambdaEvent value) override
    {
        value.func();
        return true;
    }

    bool push_to_queue_delayed(std::chrono::milliseconds, LambdaEvent) override
    {
        throw std::runtime_error("Not implemented");
    }
};

class BacktestTradingGatewayTest : public Test
{
public:
    BacktestTradingGatewayTest()
    {
        el = std::make_shared<MockEventLoop>();
    }

protected:
    EventTimeseriesChannel<double> price_source_ch;
    BacktestTradingGateway trgw;

    std::shared_ptr<MockEventLoop> el;
};

// push some price
// open pos
// push some more prices
// close pos
TEST_F(BacktestTradingGatewayTest, MarketOrderOpenClosePos)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    {
        MarketOrder mo{
                "TSTUSDT",
                111.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        bool order_responded = false;
        auto order_sub = trgw.order_response_channel().subscribe(
                el,
                [&](const OrderResponseEvent & r) {
                    order_responded = true;
                    EXPECT_EQ(r.request_guid, mo.guid());
                    EXPECT_FALSE(r.reject_reason.has_value());
                    EXPECT_FALSE(r.retry);
                });

        bool trade_responded = false;
        auto trade_sub = trgw.trade_channel().subscribe(
                el,
                [&](const TradeEvent & r) {
                    trade_responded = true;
                    EXPECT_EQ(r.trade.price(), 100.);
                    EXPECT_TRUE(r.trade.fee() > 0.);
                });

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);

        EXPECT_TRUE(order_responded);
        EXPECT_TRUE(trade_responded);
    }

    price_source_ch.push(std::chrono::milliseconds{10}, 111.);
    price_source_ch.push(std::chrono::milliseconds{20}, 222.);

    {
        MarketOrder mo{
                "TSTUSDT",
                11.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{20},
        };

        bool order_responded = false;
        auto order_sub = trgw.order_response_channel().subscribe(
                el,
                [&](const OrderResponseEvent & r) {
                    order_responded = true;
                    EXPECT_EQ(r.request_guid, mo.guid());
                    EXPECT_FALSE(r.reject_reason.has_value());
                    EXPECT_FALSE(r.retry);
                });

        bool trade_responded = false;
        auto trade_sub = trgw.trade_channel().subscribe(
                el,
                [&](const TradeEvent & r) {
                    trade_responded = true;
                    EXPECT_EQ(r.trade.price(), 222.);
                    EXPECT_TRUE(r.trade.fee() > 0.);
                });

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);

        EXPECT_TRUE(order_responded);
        EXPECT_TRUE(trade_responded);
    }
}

TEST_F(BacktestTradingGatewayTest, TpslRejectIfNoPos)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    bool tpsl_ack_responded = false;
    auto tpsl_ack_sub = trgw.tpsl_response_channel().subscribe(
            el,
            [&](const TpslResponseEvent & ev) {
                tpsl_ack_responded = true;
                EXPECT_TRUE(ev.reject_reason.has_value());
            });

    Tpsl tpsl{.take_profit_price = 130., .stop_loss_price = 10.};
    Symbol test_symbol{.symbol_name = "TSTUSDT", .lot_size_filter = {.min_qty = 0.1, .max_qty = 10., .qty_step = 0.1}};
    TpslRequestEvent ev{test_symbol, tpsl};
    trgw.push_tpsl_request(ev);

    EXPECT_TRUE(tpsl_ack_responded);
}

TEST_F(BacktestTradingGatewayTest, TpslTriggerTp)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    // open pos
    {
        MarketOrder mo{
                "TSTUSDT",
                111.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);
    }

    bool tpsl_ack_responded = false;
    auto tpsl_ack_sub = trgw.tpsl_response_channel().subscribe(
            el,
            [&](const TpslResponseEvent & ev) {
                tpsl_ack_responded = true;
                EXPECT_FALSE(ev.reject_reason.has_value());
            });

    std::optional<TpslUpdatedEvent> tpsl_upd_response;
    constexpr int tp_price = 120;
    auto tpsl_upd_sub = trgw.tpsl_updated_channel().subscribe(
            el,
            [&](const TpslUpdatedEvent & ev) {
                tpsl_upd_response = ev;
            });

    bool tp_trade_responded = false;
    auto tp_trade_sub = trgw.trade_channel().subscribe(
            el,
            [&](const TradeEvent & ev) {
                tp_trade_responded = true;
                EXPECT_EQ(ev.trade.price(), tp_price);
            });

    Tpsl tpsl{.take_profit_price = tp_price, .stop_loss_price = 50.};
    Symbol test_symbol{.symbol_name = "TSTUSDT", .lot_size_filter = {.min_qty = 0.1, .max_qty = 10., .qty_step = 0.1}};
    TpslRequestEvent ev{test_symbol, tpsl};
    trgw.push_tpsl_request(ev);

    EXPECT_TRUE(tpsl_ack_responded);
    ASSERT_TRUE(tpsl_upd_response.has_value());
    EXPECT_TRUE(tpsl_upd_response->set_up);
    EXPECT_FALSE(tp_trade_responded);
    tpsl_upd_response = {};

    // triggering TP
    price_source_ch.push(std::chrono::milliseconds{2}, 130.);

    ASSERT_TRUE(tpsl_upd_response.has_value());
    EXPECT_FALSE(tpsl_upd_response->set_up);
    EXPECT_TRUE(tp_trade_responded);
}

TEST_F(BacktestTradingGatewayTest, TpslTriggerSl)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    // open pos
    {
        MarketOrder mo{
                "TSTUSDT",
                111.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);
    }

    bool tpsl_ack_responded = false;
    auto tpsl_ack_sub = trgw.tpsl_response_channel().subscribe(
            el,
            [&](const TpslResponseEvent & ev) {
                tpsl_ack_responded = true;
                EXPECT_FALSE(ev.reject_reason.has_value());
            });

    std::optional<TpslUpdatedEvent> tpsl_upd_response;
    constexpr int sl_price = 120;
    auto tpsl_upd_sub = trgw.tpsl_updated_channel().subscribe(
            el,
            [&](const TpslUpdatedEvent & ev) {
                tpsl_upd_response = ev;
        });

    bool tp_trade_responded = false;
    auto tp_trade_sub = trgw.trade_channel().subscribe(
            el,
            [&](const TradeEvent & ev) {
                tp_trade_responded = true;
                EXPECT_EQ(ev.trade.price(), sl_price);
            });

    Tpsl tpsl{.take_profit_price = 130., .stop_loss_price = sl_price};
    Symbol test_symbol{.symbol_name = "TSTUSDT", .lot_size_filter = {.min_qty = 0.1, .max_qty = 10., .qty_step = 0.1}};
    TpslRequestEvent ev{test_symbol, tpsl};
    trgw.push_tpsl_request(ev);

    EXPECT_TRUE(tpsl_ack_responded);
    ASSERT_TRUE(tpsl_upd_response);
    EXPECT_TRUE(tpsl_upd_response->set_up);
    EXPECT_FALSE(tp_trade_responded);
    tpsl_upd_response = {};

    // triggering SL
    price_source_ch.push(std::chrono::milliseconds{2}, 40.);

    EXPECT_TRUE(tpsl_upd_response);
    EXPECT_FALSE(tpsl_upd_response->set_up);
    EXPECT_TRUE(tp_trade_responded);
}

TEST_F(BacktestTradingGatewayTest, TpslRejectIfAlreadySet)
{
}

TEST_F(BacktestTradingGatewayTest, TpslRemoveOnClosePos)
{
}

// TEST_F(BacktestTradingGatewayTest, TrailingStopRejectIfNoPos)
// {
// }
// TEST_F(BacktestTradingGatewayTest, TrailingStopPullUpAndTrigger)
// {
// }
//
// TEST_F(BacktestTradingGatewayTest, TrailingStopRemoveOnClosePos)
// {
// }

} // namespace test
