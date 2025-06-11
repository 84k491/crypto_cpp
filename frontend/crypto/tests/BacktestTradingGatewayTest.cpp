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
    std::list<std::shared_ptr<ISubscription>> subs;
};

using std::literals::operator""ms;

TEST_F(BacktestTradingGatewayTest, MarketOrder)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(1ms, 100.);

    MarketOrder mo{
            "TSTUSDT",
            111.,
            UnsignedVolume::from(1.).value(),
            Side::buy(),
            1ms,
    };

    bool order_responded = false;
    subs.push_back(trgw.order_response_channel().subscribe(
            el,
            [&](const OrderResponseEvent & r) {
                order_responded = true;
                EXPECT_EQ(r.request_guid, mo.guid());
                EXPECT_FALSE(r.reject_reason.has_value());
                EXPECT_FALSE(r.retry);
            }));

    bool trade_responded = false;
    subs.push_back(trgw.trade_channel().subscribe(
            el,
            [&](const TradeEvent & r) {
                trade_responded = true;
                EXPECT_EQ(r.trade.price(), 100.);
                EXPECT_TRUE(r.trade.fee() > 0.);
            }));

    OrderRequestEvent ev{mo};
    trgw.push_order_request(ev);

    EXPECT_TRUE(order_responded);
    EXPECT_TRUE(trade_responded);

    // TODO add some prices and make a new order
}

} // namespace test
