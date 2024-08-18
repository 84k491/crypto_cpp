#include "EventLoop.h"

#include "Events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace test {
using namespace testing;

class MockStrategy : public IEventInvoker<OrderResponseEvent, TradeEvent>
{
public:
    MockStrategy()
        : m_loop(*this)
    {
    }
    ~MockStrategy() override = default;

    void invoke(const std::variant<OrderResponseEvent, TradeEvent> & var) override
    {
        std::visit(
                VariantMatcher{
                        [&](const OrderResponseEvent &) { order_acked = true; },
                        [&](const TradeEvent &) {},
                },
                var);
    }

    EventLoop<OrderResponseEvent, TradeEvent> m_loop;

    bool order_acked = false;
};

class MockGateway : public IEventInvoker<OrderRequestEvent>
{
public:
    MockGateway()
        : m_loop(*this)
    {
    }
    ~MockGateway() override = default;

    void invoke(const std::variant<OrderRequestEvent> & var) override
    {
        std::visit(
                VariantMatcher{
                        [&](const OrderRequestEvent & order) {
                            trade_consumer = order.trade_ev_consumer;
                            order.response_consumer->push(
                                    OrderResponseEvent(order.order.guid()));
                        },
                },
                var);
    }

    EventLoop<OrderRequestEvent> m_loop;

    IEventConsumer<TradeEvent> * trade_consumer = nullptr;
};

class EventLoopTest : public Test
{
public:
protected:
};

TEST_F(EventLoopTest, EventLoopTest)
{
    std::unique_ptr<MockStrategy> strategy;
    MockGateway gateway;

    // strategy pushes order to gw
    gateway.m_loop.as_consumer<OrderRequestEvent>().push(
            OrderRequestEvent{
                    MarketOrder{
                            "BTCUSDT",
                            1.1,
                            SignedVolume{1.},
                            std::chrono::milliseconds{1}},
                    strategy->m_loop,
                    strategy->m_loop,
            });

    // gw pushes response to strategy
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // destroy strategy
    strategy.reset();

    // gw pushes trade to strategy
    gateway.trade_consumer->push(
            TradeEvent{
                    Trade{
                            std::chrono::milliseconds{1},
                            "BTCUSDT",
                            1.1,
                            UnsignedVolume::from(1.1).value(),
                            Side::Buy,
                            0.01},
            });

    // no segfault
}

} // namespace test
