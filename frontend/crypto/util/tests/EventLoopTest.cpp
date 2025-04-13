#include "EventLoop.h"

#include "EventChannel.h"
#include "EventLoopSubscriber.h"
#include "Events.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

namespace test {
using namespace testing;

class MockStrategy
{
public:
    MockStrategy()
    {
        m_invoker_sub = m_loop.invoker().register_invoker<OrderResponseEvent>([&](const auto &) {
            order_acked = true;
        });
    }
    ~MockStrategy() = default;

    bool order_acked = false;

    EventLoopSubscriber<OrderResponseEvent, TradeEvent> m_loop;
    std::shared_ptr<ISubscription> m_invoker_sub;
};

class MockGateway
{
public:
    MockGateway()
    {
        m_invoker_sub = m_loop.invoker().register_invoker<OrderRequestEvent>([&](const auto & order) {
            m_order_response_channel.push(OrderResponseEvent(order.order.symbol(), order.order.guid()));
        });
    }
    ~MockGateway() = default;

    std::weak_ptr<IEventConsumer<TradeEvent>> trade_consumer;

    EventLoopSubscriber<OrderRequestEvent> m_loop;
    EventChannel<OrderResponseEvent> m_order_response_channel;
    std::shared_ptr<ISubscription> m_invoker_sub;
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
    gateway.m_loop.push_event(OrderRequestEvent{
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
