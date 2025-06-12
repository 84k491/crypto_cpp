#include "ByBitTradingGateway.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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

class BybitTradingGatewayLiveTest : public Test
{
public:
    BybitTradingGatewayLiveTest()
    {
        el = std::make_shared<MockEventLoop>();
    }

protected:
    std::shared_ptr<MockEventLoop> el;
    std::mutex mutex;
};

TEST_F(BybitTradingGatewayLiveTest, OpenAndClosePos)
{
    ByBitTradingGateway trgw;
    std::condition_variable order_cv;
    std::condition_variable trade_cv;

    std::optional<OrderResponseEvent> order_response;
    std::shared_ptr<ISubscription> order_sub = trgw.order_response_channel().subscribe(
            el,
            [this, &order_response, &order_cv](const OrderResponseEvent & ev) {
                std::lock_guard<std::mutex> lock(mutex);
                order_response = ev;
                EXPECT_FALSE(order_response->reject_reason.has_value());
                order_cv.notify_all();
            });

    std::optional<TradeEvent> trade_response;
    std::shared_ptr<ISubscription> trade_sub = trgw.trade_channel().subscribe(
            el,
            [this, &trade_response, &trade_cv](const TradeEvent & ev) {
                std::lock_guard<std::mutex> lock(mutex);
                trade_response = ev;

                trade_cv.notify_all();
            });

    {
        MarketOrder mo{
                "BTCUSDT",
                100.,
                UnsignedVolume::from(0.002).value(),
                Side::buy(),
                std::chrono::milliseconds{1}};
        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        order_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&order_response] { return order_response.has_value(); });
        ASSERT_TRUE(order_response.has_value());
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        trade_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&trade_response] { return trade_response.has_value(); });
        ASSERT_TRUE(trade_response.has_value());
    }

    order_response = {};
    trade_response = {};

    // closing pos

    {
        MarketOrder mo{
                "BTCUSDT",
                100.,
                UnsignedVolume::from(0.002).value(),
                Side::sell(),
                std::chrono::milliseconds{1}};
        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        order_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&order_response] { return order_response.has_value(); });
        ASSERT_TRUE(order_response.has_value());
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        trade_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&trade_response] { return trade_response.has_value(); });
        ASSERT_TRUE(trade_response.has_value());
    }

    // TODO check opened pos
}

} // namespace test
