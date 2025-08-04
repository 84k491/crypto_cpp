#include "ByBitTradingGateway.h"
#include "ConditionalOrders.h"
#include "Events.h"

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
        EXPECT_FALSE(order_response->reject_reason.has_value());
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

TEST_F(BybitTradingGatewayLiveTest, CloseWithTpsl)
{
    ByBitTradingGateway trgw;
    std::condition_variable order_cv;
    std::condition_variable trade_cv;
    std::condition_variable tpsl_resp_cv;
    std::condition_variable tpsl_upd_cv;

    std::optional<OrderResponseEvent> order_response;
    std::shared_ptr<ISubscription> order_sub = trgw.order_response_channel().subscribe(
            el,
            [this, &order_response, &order_cv](const OrderResponseEvent & ev) {
                std::lock_guard<std::mutex> lock(mutex);
                order_response = ev;
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

    std::optional<TpslUpdatedEvent> tpsl_response;
    std::shared_ptr<ISubscription> tpsl_r_sub = trgw.tpsl_updated_channel().subscribe(
            el,
            [&](const TpslUpdatedEvent & ev) {
                std::lock_guard<std::mutex> lock(mutex);
                tpsl_response = ev;

                tpsl_resp_cv.notify_all();
            });

    std::optional<TpslUpdatedEvent> tpsl_update;
    std::shared_ptr<ISubscription> tpsl_upd_sub = trgw.tpsl_updated_channel().subscribe(
            el,
            [&](const TpslUpdatedEvent & ev) {
                std::lock_guard<std::mutex> lock(mutex);
                tpsl_update = ev;

                tpsl_upd_cv.notify_all();
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
        EXPECT_FALSE(order_response->reject_reason.has_value());
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        trade_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&trade_response] { return trade_response.has_value(); });
        ASSERT_TRUE(trade_response.has_value());
    }

    const auto entry_price = trade_response->trade.price();
    TpslFullPos::Prices tpsl{.take_profit_price = entry_price + 500, .stop_loss_price = entry_price - 500};
    TpslRequestEvent tpsl_req_ev{
            Symbol{.symbol_name = "BTCUSDT", .lot_size_filter = {}},
            tpsl.take_profit_price,
            tpsl.stop_loss_price};
    trgw.push_tpsl_request(tpsl_req_ev);

    order_response = {};
    trade_response = {};

    {
        std::unique_lock<std::mutex> lock(mutex);
        tpsl_resp_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&] { return tpsl_response.has_value(); });
        ASSERT_TRUE(tpsl_response.has_value());
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        tpsl_upd_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&] { return tpsl_update.has_value(); });
        ASSERT_TRUE(tpsl_update.has_value());
        EXPECT_TRUE(tpsl_update->set_up);
    }

    tpsl_response = {};
    tpsl_update = {};

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

    // there must be a tpsl update (cancelled) on position close
    {
        std::unique_lock<std::mutex> lock(mutex);
        tpsl_upd_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&] { return tpsl_update.has_value(); });
        ASSERT_TRUE(tpsl_update.has_value());
        EXPECT_FALSE(tpsl_update->set_up);
    }

    // TODO check opened pos
}

TEST_F(BybitTradingGatewayLiveTest, TpslRejectIfNoPos)
{
    // TODO check opened pos

    ByBitTradingGateway trgw;
    std::condition_variable tpsl_resp_cv;
    std::condition_variable tpsl_upd_cv;

    std::optional<TpslUpdatedEvent> tpsl_response;
    std::shared_ptr<ISubscription> tpsl_r_sub = trgw.tpsl_updated_channel().subscribe(
            el,
            [&](const TpslUpdatedEvent & ev) {
                std::lock_guard<std::mutex> lock(mutex);
                tpsl_response = ev;

                tpsl_resp_cv.notify_all();
            });

    TpslFullPos::Prices tpsl{.take_profit_price = 110'000, .stop_loss_price = 105'000};
    TpslRequestEvent tpsl_req_ev{
            Symbol{.symbol_name = "BTCUSDT", .lot_size_filter = {}},
            tpsl.take_profit_price,
            tpsl.stop_loss_price};
    trgw.push_tpsl_request(tpsl_req_ev);

    {
        std::unique_lock<std::mutex> lock(mutex);
        tpsl_resp_cv.wait_for(
                lock,
                std::chrono::seconds(10),
                [&] { return tpsl_response.has_value(); });
        ASSERT_TRUE(tpsl_response.has_value());
    }

    EXPECT_EQ(tpsl_response->reject_reason, "can not set tp/sl/ts for zero position");
}

} // namespace test
