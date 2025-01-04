#include "EventLoop.h"

#include "EventLoopSubscriber.h"
#include "EventChannel.h"
#include "Events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

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

    bool order_acked = false;

    EventLoopSubscriber<OrderResponseEvent, TradeEvent> m_loop;
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
                            m_order_response_channel.push(OrderResponseEvent(order.order.symbol(), order.order.guid()));
                        },
                },
                var);
    }

    std::weak_ptr<IEventConsumer<TradeEvent>> trade_consumer;

    EventLoopSubscriber<OrderRequestEvent> m_loop;
    EventChannel<OrderResponseEvent> m_order_response_channel;
};

class EventLoopTest : public Test
{
public:
protected:
};

// check that gateway don't crash if strategy is destroyed
TEST_F(EventLoopTest, StrategyDestruction)
{
    auto strategy = std::make_unique<MockStrategy>();
    MockGateway gateway;
    strategy->m_loop.subscribe(gateway.m_order_response_channel);

    // strategy pushes order to gw
    gateway.m_loop.push_event(            OrderRequestEvent{
                    MarketOrder{
                            "BTCUSDT",
                            1.1,
                            SignedVolume{1.},
                            std::chrono::milliseconds{1}},
            });

    // gw pushes response to strategy
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // destroy strategy
    strategy.reset();

    ASSERT_TRUE(gateway.trade_consumer.expired());
}

// TODO add test for delayed events

} // namespace test
