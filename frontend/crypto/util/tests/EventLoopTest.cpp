#include "EventLoop.h"

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
        : m_loop(EventLoop<OrderResponseEvent, TradeEvent>::create(*this))
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

    std::shared_ptr<EventLoop<OrderResponseEvent, TradeEvent>> m_loop;
};

class MockGateway : public IEventInvoker<OrderRequestEvent>
{
public:
    MockGateway()
        : m_loop(EventLoop<OrderRequestEvent>::create(*this))
    {
    }
    ~MockGateway() override = default;

    void invoke(const std::variant<OrderRequestEvent> & var) override
    {
        std::visit(
                VariantMatcher{
                        [&](const OrderRequestEvent & order) {
                            trade_consumer = order.trade_ev_consumer;
                            const auto p = order.response_consumer.lock();
                            ASSERT_TRUE(p);
                            p->push(OrderResponseEvent(order.order.symbol(), order.order.guid()));
                        },
                },
                var);
    }

    std::weak_ptr<IEventConsumer<TradeEvent>> trade_consumer;

    std::shared_ptr<EventLoop<OrderRequestEvent>> m_loop;
};

class MockSlowGateway : public IEventInvoker<OrderRequestEvent>
{
public:
    MockSlowGateway()
        : m_loop(EventLoop<OrderRequestEvent>::create(*this))
    {
    }
    ~MockSlowGateway() override = default;

    void invoke(const std::variant<OrderRequestEvent> & var) override
    {
        std::visit(
                VariantMatcher{
                        [&](const OrderRequestEvent & order) {
                            trade_consumer = order.trade_ev_consumer;
                            const auto p = order.response_consumer.lock();
                            ASSERT_TRUE(p) << "We need to gain ownership here";
                            std::this_thread::sleep_for(std::chrono::milliseconds{10});
                            p->push(OrderResponseEvent(order.order.symbol(), order.order.guid()));
                            std::this_thread::sleep_for(std::chrono::milliseconds{10});
                        },
                },
                var);
    }

    std::weak_ptr<IEventConsumer<TradeEvent>> trade_consumer;

    std::shared_ptr<EventLoop<OrderRequestEvent>> m_loop;
};

class MockSlowStrategy : public IEventInvoker<OrderResponseEvent, TradeEvent>
{
public:
    MockSlowStrategy()
        : m_loop(EventLoop<OrderResponseEvent, TradeEvent>::create(*this))
    {
    }
    ~MockSlowStrategy() override = default;

    void invoke(const std::variant<OrderResponseEvent, TradeEvent> & var) override
    {
        std::visit(
                VariantMatcher{
                        [&](const OrderResponseEvent &) {
                            // trying to trigger a segfault
                            some_str = "order_ack_received";
                        },
                        [&](const TradeEvent &) {},
                },
                var);
    }

    std::string some_str = "inited_string";

    std::shared_ptr<EventLoop<OrderResponseEvent, TradeEvent>> m_loop;
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

    // strategy pushes order to gw
    gateway.m_loop->as_consumer<OrderRequestEvent>().push(
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

    ASSERT_TRUE(gateway.trade_consumer.expired());
}

TEST_F(EventLoopTest, DanglingInvokerRefCheck)
{
    // create obj with sp<event loop>
    auto strategy = std::make_unique<MockSlowStrategy>();

    MockSlowGateway gateway;

    // push ev to obj2
    gateway.m_loop->as_consumer<OrderRequestEvent>().push(
            OrderRequestEvent{
                    MarketOrder{
                            "BTCUSDT",
                            1.1,
                            SignedVolume{1.},
                            std::chrono::milliseconds{1}},
                    strategy->m_loop,
                    strategy->m_loop,
            });
    // obj2 locks wptr immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // obj2 takes some time to start handling ev

    // destroy obj1, release event loop sp, obj2 now owns obj1's event loop. ref to invoker dangles
    strategy.reset();
    // obj2 pushes reponse to event loop, waits to prevent event loop destroying
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // event loop gets response, invokes it with dangling ref
    // SEGAFULT!!
}

} // namespace test
