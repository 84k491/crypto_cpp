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

    std::shared_ptr<EventLoop<OrderResponseEvent, TradeEvent>> m_loop;

    bool order_acked = false;
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

    std::shared_ptr<EventLoop<OrderRequestEvent>> m_loop;

    std::weak_ptr<IEventConsumer<TradeEvent>> trade_consumer;
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

} // namespace test
