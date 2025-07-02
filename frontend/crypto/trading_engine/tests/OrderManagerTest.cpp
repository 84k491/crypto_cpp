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
        constexpr double fee = .5;

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

    void push_tpsl_request(const TpslRequestEvent & ) override
    {
        throw std::runtime_error("not implemented");
    }

    void push_trailing_stop_request(const TrailingStopLossRequestEvent & ) override
    {
        throw std::runtime_error("not implemented");
    }

    void push_take_profit_request(const TakeProfitMarketOrder & tp) override
    {
    }

    void push_stop_loss_request(const StopLossMarketOrder & sl) override
    {
    }

    void cancel_stop_loss_request(xg::Guid guid) override
    {
    }

    void cancel_take_profit_request(xg::Guid guid) override
    {
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
    ASSERT_EQ(order_manager.pending_orders().size(), 0);
}

} // namespace test
