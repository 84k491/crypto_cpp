#include "OrderManager.h"

#include "EventBarrier.h"
#include "ITradingGateway.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace test {

class OrderManagerTest
    : public testing::Test
    , public ITradingGateway
{
public:
    static constexpr double fee = .5;

    OrderManagerTest()
        : order_manager(
                  Symbol{
                          "TSTUSDT",
                          Symbol::LotSizeFilter{
                                  0.001,
                                  1e6,
                                  0.01}},
                  event_loop,
                  *this)
    {
    }

    void push_order_request(const OrderRequestEvent & order) override
    {
        last_order_request = order;
    }

    void reply_to_order_req(const OrderRequestEvent & order)
    {
        MarketOrder o = order.order;

        Trade trade{
                o.signal_ts(),
                o.symbol(),
                o.guid(),
                o.price(),
                o.target_volume(),
                o.side(),
                fee};
        m_trade_channel.push(trade);

        OrderResponseEvent response{
                o.symbol(),
                o.guid(),
                {}};
        m_order_response_channel.push(response);
    }

    void push_tpsl_request(const TpslRequestEvent &) override
    {
        throw std::runtime_error("not implemented");
    }

    void push_trailing_stop_request(const TrailingStopLossRequestEvent &) override
    {
        throw std::runtime_error("not implemented");
    }

    void push_take_profit_request(const TakeProfitMarketOrder & tp) override
    {
        last_take_profit_request = tp;
    }
    void reply_to_take_profit(const TakeProfitMarketOrder & tp)
    {
        m_tp_updated_channel.push({tp.guid(), true});
    }
    void trade_take_profit(const TakeProfitMarketOrder & tp)
    {
        Trade trade{
                tp.signal_ts(),
                tp.symbol(),
                tp.guid(),
                tp.price(),
                tp.target_volume(),
                tp.side(),
                fee};
        m_trade_channel.push(trade);
    }
    void cancel_take_profit(const TakeProfitMarketOrder & tp)
    {
        m_tp_updated_channel.push({tp.guid(), false});
    }

    void push_stop_loss_request(const StopLossMarketOrder & sl) override
    {
    }

    void cancel_stop_loss_request(xg::Guid guid) override
    {
    }

    void cancel_take_profit_request(xg::Guid guid) override
    {
        last_cancel_take_profit_request = guid;
    }

    EventChannel<OrderResponseEvent> & order_response_channel() override
    {
        return m_order_response_channel;
    }

    EventChannel<TradeEvent> & trade_channel() override
    {
        return m_trade_channel;
    }

    EventChannel<TpslResponseEvent> & tpsl_response_channel() override
    {
        throw std::runtime_error("not implemented");
    }

    EventChannel<TpslUpdatedEvent> & tpsl_updated_channel() override
    {
        throw std::runtime_error("not implemented");
    }

    EventChannel<TrailingStopLossResponseEvent> & trailing_stop_response_channel() override
    {
        throw std::runtime_error("not implemented");
    }

    EventChannel<TrailingStopLossUpdatedEvent> & trailing_stop_update_channel() override
    {
        throw std::runtime_error("not implemented");
    }

    EventChannel<TakeProfitUpdatedEvent> & take_profit_update_channel() override
    {
        return m_tp_updated_channel;
    }

    EventChannel<StopLossUpdatedEvent> & stop_loss_update_channel() override
    {
        return m_sl_updated_channel;
    }

protected:
    // ITradingGateway
    EventChannel<OrderResponseEvent> m_order_response_channel;
    EventChannel<TradeEvent> m_trade_channel;
    EventChannel<TakeProfitUpdatedEvent> m_tp_updated_channel;
    EventChannel<StopLossUpdatedEvent> m_sl_updated_channel;

    // Test
    EventLoopSubscriber event_loop;
    OrderManager order_manager;

    std::optional<OrderRequestEvent> last_order_request;
    std::optional<TakeProfitMarketOrder> last_take_profit_request;
    std::optional<xg::Guid> last_cancel_take_profit_request;

    EventChannel<BarrierEvent> m_barrier_channel;
};

using namespace std::chrono_literals;

TEST_F(OrderManagerTest, MarketOrderFill)
{
    auto & order_ch = order_manager.send_market_order(111, SignedVolume{1}, 1ms);
    auto order_ptr = order_ch.get();

    auto sub_ptr = order_ch.subscribe(
            event_loop.m_event_loop,
            [&](const auto & o) {
                order_ptr = o;
            });

    ASSERT_TRUE(order_ptr);
    ASSERT_TRUE(last_order_request.has_value());
    ASSERT_EQ(order_ptr->status(), OrderStatus::Pending);
    ASSERT_EQ(order_manager.pending_orders().size(), 1);

    reply_to_order_req(last_order_request.value());
    last_order_request.reset();

    EventBarrier barrier{event_loop, m_barrier_channel};
    barrier.wait();

    ASSERT_EQ(order_ptr->status(), OrderStatus::Filled);
    ASSERT_EQ(order_ptr->fee(), fee);
    ASSERT_EQ(order_manager.pending_orders().size(), 0);
}

TEST_F(OrderManagerTest, MarketOrderNoSub)
{
    order_manager.send_market_order(111, SignedVolume{1}, 1ms);

    ASSERT_TRUE(last_order_request.has_value());

    reply_to_order_req(last_order_request.value());

    EventBarrier barrier{event_loop, m_barrier_channel};
    barrier.wait();

    ASSERT_EQ(order_manager.pending_orders().size(), 0);
}

TEST_F(OrderManagerTest, TakeProfitSuspendAndFill)
{
    auto & ch = order_manager.send_take_profit(111, SignedVolume{1}, 1ms);
    std::shared_ptr<TakeProfitMarketOrder> tp = nullptr;
    auto tp_sub = ch.subscribe(
            event_loop.m_event_loop,
            [&](const std::shared_ptr<TakeProfitMarketOrder> & tp_ptr) {
                tp = tp_ptr;
            });
    tp = ch.get();
    ASSERT_TRUE(tp);
    ASSERT_EQ(tp->status(), OrderStatus::Pending);
    ASSERT_TRUE(last_take_profit_request.has_value());
    ASSERT_DOUBLE_EQ(tp->target_volume().value(), 1.);
    ASSERT_EQ(order_manager.conditionals(), 1);

    {
        reply_to_take_profit(last_take_profit_request.value());
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    ASSERT_EQ(tp->status(), OrderStatus::Suspended);
    ASSERT_EQ(tp->suspended_volume(), tp->target_volume());

    {
        trade_take_profit(last_take_profit_request.value());
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    EXPECT_EQ(tp->status(), OrderStatus::Filled);
    EXPECT_EQ(tp->suspended_volume(), UnsignedVolume{});
    EXPECT_EQ(tp->filled_volume(), tp->target_volume());

    EXPECT_EQ(order_manager.conditionals(), 1);
    tp_sub.reset();
    EXPECT_EQ(order_manager.conditionals(), 0);
}

TEST_F(OrderManagerTest, TakeProfitSuspendAndCancel)
{
    auto & ch = order_manager.send_take_profit(111, SignedVolume{1}, 1ms);
    std::shared_ptr<TakeProfitMarketOrder> tp = nullptr;
    auto tp_sub = ch.subscribe(
            event_loop.m_event_loop,
            [&](const std::shared_ptr<TakeProfitMarketOrder> & tp_ptr) {
                tp = tp_ptr;
            });
    tp = ch.get();
    ASSERT_TRUE(tp);
    ASSERT_EQ(tp->status(), OrderStatus::Pending);
    ASSERT_TRUE(last_take_profit_request.has_value());
    ASSERT_DOUBLE_EQ(tp->target_volume().value(), 1.);
    ASSERT_EQ(order_manager.conditionals(), 1);

    {
        reply_to_take_profit(last_take_profit_request.value());
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    ASSERT_EQ(tp->status(), OrderStatus::Suspended);
    ASSERT_EQ(tp->suspended_volume(), tp->target_volume());

    // TODO cancel
    order_manager.cancel_take_profit(last_take_profit_request->guid());

    {
        cancel_take_profit(*tp);
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    EXPECT_EQ(tp->status(), OrderStatus::Cancelled);
    EXPECT_EQ(tp->suspended_volume(), UnsignedVolume{});
    ASSERT_DOUBLE_EQ(tp->target_volume().value(), 0.);
    EXPECT_EQ(tp->filled_volume(), tp->target_volume());

    EXPECT_EQ(order_manager.conditionals(), 1);
    tp_sub.reset();
    EXPECT_EQ(order_manager.conditionals(), 0);
}

TEST_F(OrderManagerTest, TakeProfitCancelOnChannelDrop)
{
    // TODO
}

} // namespace test
