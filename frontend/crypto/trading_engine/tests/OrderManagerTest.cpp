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
                          .symbol_name = "TSTUSDT",
                          .lot_size_filter = Symbol::LotSizeFilter{
                                  .min_qty = 0.001,
                                  .max_qty = 1e6,
                                  .qty_step = 0.01}},
                  event_loop,
                  *this)
    {
    }

    void push_order_request(const OrderRequestEvent & order) override
    {
        last_order_request = order;
    }

    void ack_order_req(const OrderRequestEvent & order)
    {
        MarketOrder o = order.order;

        OrderResponseEvent response{
                o.symbol(),
                o.guid(),
                {}};
        m_order_response_channel.push(response);
    }
    void trade_market_order(const OrderRequestEvent & order)
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
    void push_stop_loss_request(const StopLossMarketOrder & sl) override
    {
        last_stop_loss_request = sl;
    }
    void reply_to_stop_loss(const StopLossMarketOrder & sl)
    {
        m_sl_updated_channel.push({sl.guid(), true});
    }
    void trade_stop_loss(const StopLossMarketOrder & sl)
    {
        Trade trade{
                sl.signal_ts(),
                sl.symbol(),
                sl.guid(),
                sl.price(),
                sl.target_volume(),
                sl.side(),
                fee};
        m_trade_channel.push(trade);
    }

    void cancel_take_profit_request(xg::Guid guid) override
    {
        last_cancel_take_profit_request = guid;
    }
    void cancel_take_profit(const TakeProfitMarketOrder & tp)
    {
        m_tp_updated_channel.push({tp.guid(), false});
    }

    void cancel_stop_loss_request(xg::Guid guid) override
    {
        last_cancel_stop_loss_request = guid;
    }
    void cancel_stop_loss(const StopLossMarketOrder & sl)
    {
        m_sl_updated_channel.push({sl.guid(), false});
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
    std::optional<StopLossMarketOrder> last_stop_loss_request;

    std::optional<xg::Guid> last_cancel_take_profit_request;
    std::optional<xg::Guid> last_cancel_stop_loss_request;

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

    trade_market_order(last_order_request.value());
    ack_order_req(last_order_request.value());
    last_order_request.reset();

    EventBarrier barrier{event_loop, m_barrier_channel};
    barrier.wait();

    ASSERT_EQ(order_ptr->status(), OrderStatus::Filled);
    ASSERT_EQ(order_ptr->fee(), fee);
    ASSERT_EQ(order_manager.pending_orders().size(), 0);
}

TEST_F(OrderManagerTest, MarketOrderFillTradeAfterAck)
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

    ack_order_req(last_order_request.value());
    trade_market_order(last_order_request.value());
    last_order_request.reset();

    EventBarrier barrier{event_loop, m_barrier_channel};
    barrier.wait();

    sub_ptr.reset();

    EXPECT_EQ(order_ptr->status(), OrderStatus::Filled);
    EXPECT_EQ(order_ptr->fee(), fee);
    EXPECT_EQ(order_manager.pending_orders().size(), 0);
}

TEST_F(OrderManagerTest, MarketOrderNoSub)
{
    order_manager.send_market_order(111, SignedVolume{1}, 1ms);

    ASSERT_TRUE(last_order_request.has_value());

    trade_market_order(last_order_request.value());
    ack_order_req(last_order_request.value());

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

    order_manager.cancel_take_profit(last_take_profit_request->guid());
    ASSERT_TRUE(tp->is_cancel_requested());

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

    EXPECT_EQ(order_manager.conditionals(), 1);
    tp_sub.reset();
    EXPECT_EQ(order_manager.conditionals(), 0);
    ASSERT_TRUE(tp->is_cancel_requested());
}

TEST_F(OrderManagerTest, StopLossSuspendAndFill)
{
    auto & ch = order_manager.send_stop_loss(111, SignedVolume{1}, 1ms);
    std::shared_ptr<StopLossMarketOrder> sl = nullptr;
    auto sl_sub = ch.subscribe(
            event_loop.m_event_loop,
            [&](const std::shared_ptr<StopLossMarketOrder> & sl_ptr) {
                sl = sl_ptr;
            });
    sl = ch.get();
    ASSERT_TRUE(sl);
    ASSERT_EQ(sl->status(), OrderStatus::Pending);
    ASSERT_TRUE(last_stop_loss_request.has_value());
    ASSERT_DOUBLE_EQ(sl->target_volume().value(), 1.);
    ASSERT_EQ(order_manager.conditionals(), 1);

    {
        reply_to_stop_loss(last_stop_loss_request.value());
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    ASSERT_EQ(sl->status(), OrderStatus::Suspended);
    ASSERT_EQ(sl->suspended_volume(), sl->target_volume());

    {
        trade_stop_loss(last_stop_loss_request.value());
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    EXPECT_EQ(sl->status(), OrderStatus::Filled);
    EXPECT_EQ(sl->suspended_volume(), UnsignedVolume{});
    EXPECT_EQ(sl->filled_volume(), sl->target_volume());

    EXPECT_EQ(order_manager.conditionals(), 1);
    sl_sub.reset();
    EXPECT_EQ(order_manager.conditionals(), 0);
}

TEST_F(OrderManagerTest, StopLossSuspendAndCancel)
{
    auto & ch = order_manager.send_stop_loss(111, SignedVolume{1}, 1ms);
    std::shared_ptr<StopLossMarketOrder> sl = nullptr;
    auto tp_sub = ch.subscribe(
            event_loop.m_event_loop,
            [&](const std::shared_ptr<StopLossMarketOrder> & sl_ptr) {
                sl = sl_ptr;
            });
    sl = ch.get();
    ASSERT_TRUE(sl);
    ASSERT_EQ(sl->status(), OrderStatus::Pending);
    ASSERT_TRUE(last_stop_loss_request.has_value());
    ASSERT_DOUBLE_EQ(sl->target_volume().value(), 1.);
    ASSERT_EQ(order_manager.conditionals(), 1);

    {
        reply_to_stop_loss(last_stop_loss_request.value());
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    ASSERT_EQ(sl->status(), OrderStatus::Suspended);
    ASSERT_EQ(sl->suspended_volume(), sl->target_volume());

    order_manager.cancel_stop_loss(last_stop_loss_request->guid());
    ASSERT_TRUE(sl->is_cancel_requested());

    {
        cancel_stop_loss(*sl);
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    EXPECT_EQ(sl->status(), OrderStatus::Cancelled);
    EXPECT_EQ(sl->suspended_volume(), UnsignedVolume{});
    ASSERT_DOUBLE_EQ(sl->target_volume().value(), 0.);
    EXPECT_EQ(sl->filled_volume(), sl->target_volume());

    EXPECT_EQ(order_manager.conditionals(), 1);
    tp_sub.reset();
    EXPECT_EQ(order_manager.conditionals(), 0);
}

TEST_F(OrderManagerTest, StopLossCancelOnChannelDrop)
{
    auto & ch = order_manager.send_stop_loss(111, SignedVolume{1}, 1ms);
    std::shared_ptr<StopLossMarketOrder> sl = nullptr;
    auto tp_sub = ch.subscribe(
            event_loop.m_event_loop,
            [&](const std::shared_ptr<StopLossMarketOrder> & sl_ptr) {
                sl = sl_ptr;
            });
    sl = ch.get();
    ASSERT_TRUE(sl);
    ASSERT_EQ(sl->status(), OrderStatus::Pending);
    ASSERT_TRUE(last_stop_loss_request.has_value());
    ASSERT_DOUBLE_EQ(sl->target_volume().value(), 1.);
    ASSERT_EQ(order_manager.conditionals(), 1);

    {
        reply_to_stop_loss(last_stop_loss_request.value());
        EventBarrier barrier{event_loop, m_barrier_channel};
        barrier.wait();
    }

    ASSERT_EQ(sl->status(), OrderStatus::Suspended);
    ASSERT_EQ(sl->suspended_volume(), sl->target_volume());

    EXPECT_EQ(order_manager.conditionals(), 1);
    tp_sub.reset();
    EXPECT_EQ(order_manager.conditionals(), 0);
    ASSERT_TRUE(sl->is_cancel_requested());
}

} // namespace test
