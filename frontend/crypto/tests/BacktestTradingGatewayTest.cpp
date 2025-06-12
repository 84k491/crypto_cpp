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

} // namespace test
