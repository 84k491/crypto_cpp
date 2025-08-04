#include "StrategyInstance.h"

#include "Events.h"
#include "MockStrategy.h"
#include "TpslExitStrategy.h"
#include "Trade.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace test {
using namespace testing;

class MockMDGateway : public IMarketDataGateway
{
public:
    MockMDGateway()
    {
        m_status.push(WorkStatus::Stopped);
    }

    ~MockMDGateway() override = default;

    void push_async_request(HistoricalMDRequest &&) override
    {
    }

    EventChannel<HistoricalMDGeneratorEvent> & historical_prices_channel() override
    {
        return m_historical_channel;
    }

    EventChannel<MDPriceEvent> & live_prices_channel() override
    {
        return m_live_prices_channel;
    }

    void push_async_request(LiveMDRequest && request) override
    {
        ++m_live_requests_count;
        m_last_live_request = std::move(request);
    }

    void unsubscribe_from_live(xg::Guid) override
    {
        ++m_unsubscribed_count;
    }

    EventObjectChannel<WorkStatus> & status_channel() override
    {
        return m_status;
    }

    size_t live_requests_count() const { return m_live_requests_count; }
    size_t unsubscribed_count() const { return m_unsubscribed_count; }

public:
    std::optional<LiveMDRequest> m_last_live_request;

private:
    EventObjectChannel<WorkStatus> m_status;

    size_t m_live_requests_count = 0;
    size_t m_unsubscribed_count = 0;

    EventChannel<HistoricalMDGeneratorEvent> m_historical_channel;
    EventChannel<MDPriceEvent> m_live_prices_channel;
};

class MockTradingGateway : public ITradingGateway
{
public:
    ~MockTradingGateway() override = default;

    void push_order_request(const OrderRequestEvent & order) override
    {
        m_last_order_request = order;
    }

    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override
    {
        m_last_tpsl_request = tpsl_ev;
    }

    void push_take_profit_request(const TakeProfitMarketOrder &) override
    {
        throw std::runtime_error{"not implemented"};
    }

    void push_stop_loss_request(const StopLossMarketOrder &) override
    {
        throw std::runtime_error{"not implemented"};
    }

    void cancel_stop_loss_request(xg::Guid) override
    {
        throw std::runtime_error{"not implemented"};
    }

    void cancel_take_profit_request(xg::Guid) override
    {
        throw std::runtime_error{"not implemented"};
    }

    void push_trailing_stop_request(const TrailingStopLossRequestEvent &) override
    {
        // not implemented
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
        return m_tpsl_response_channel;
    }
    EventChannel<TpslUpdatedEvent> & tpsl_updated_channel() override
    {
        return m_tpsl_updated_channel;
    }

    EventChannel<TrailingStopLossUpdatedEvent> & trailing_stop_update_channel() override
    {
        return m_tsl_updated_channel;
    }

    EventChannel<TakeProfitUpdatedEvent> & take_profit_update_channel() override
    {
        return m_take_profit_update_channel;
    }

    EventChannel<StopLossUpdatedEvent> & stop_loss_update_channel() override
    {
        return m_stop_loss_update_channel;
    }

public:
    std::optional<OrderRequestEvent> m_last_order_request;
    std::optional<TpslRequestEvent> m_last_tpsl_request;

    EventChannel<OrderResponseEvent> m_order_response_channel;
    EventChannel<TradeEvent> m_trade_channel;
    EventChannel<TpslResponseEvent> m_tpsl_response_channel;
    EventChannel<TpslUpdatedEvent> m_tpsl_updated_channel;
    EventChannel<TrailingStopLossUpdatedEvent> m_tsl_updated_channel;
    EventChannel<TakeProfitUpdatedEvent> m_take_profit_update_channel;
    EventChannel<StopLossUpdatedEvent> m_stop_loss_update_channel;
};

class MockEventConsumer : public ILambdaAcceptor
{
    bool push_to_queue(LambdaEvent value) override
    {
        value.func();
        return true;
    }

    bool push_to_queue_delayed(std::chrono::milliseconds, LambdaEvent) override
    {
        EXPECT_TRUE(false) << "Not implemented";
        throw std::runtime_error("Not implemented");
    }
};

class StrategyInstanceTest : public Test
{
public:
    StrategyInstanceTest()
        : event_consumer(std::make_shared<MockEventConsumer>())
        , m_symbol("BTCUSD")
        , strategy_instance(nullptr)
    {
        m_symbol.lot_size_filter.max_qty = 1'000'000;
        m_symbol.lot_size_filter.min_qty = 0.001;
        m_symbol.lot_size_filter.qty_step = 0.001;

        strategy_instance = std::make_unique<StrategyInstance>(
                m_symbol,
                std::nullopt,
                "Mock",
                JsonStrategyConfig{nlohmann::json{}},
                md_gateway,
                tr_gateway);

        strategy_ptr = std::dynamic_pointer_cast<MockStrategy>(strategy_instance->get_strategy());

        {
            strategy_status = strategy_instance->status_channel().get();
        }

        status_sub = strategy_instance->status_channel().subscribe(
                event_consumer,
                [&](const auto & status) {
                    strategy_status = status;
                });
    }

protected:
    std::shared_ptr<MockEventConsumer> event_consumer;
    Symbol m_symbol;

    MockMDGateway md_gateway;
    MockTradingGateway tr_gateway;

    std::shared_ptr<MockStrategy> strategy_ptr;
    std::unique_ptr<StrategyInstance> strategy_instance;

    WorkStatus strategy_status = WorkStatus::Panic;
    std::shared_ptr<EventObjectSubscription<WorkStatus>> status_sub;
};

// strategy starts in stopped state
// strategy sends MD request at start and goes to Live state
// gateway sends price, strategy gets it
// strategy still in Live state
// strategy stops
// strategy unsubscribes from gateway
// strategy goes to Stopped state
TEST_F(StrategyInstanceTest, SubForLiveMarketData_GetPrice_GracefullStop)
{
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance->run_async();
    strategy_instance->wait_event_barrier();
    ASSERT_EQ(md_gateway.live_requests_count(), 1);
    ASSERT_EQ(strategy_status, WorkStatus::Live);

    const auto live_req = md_gateway.m_last_live_request.value();

    size_t prices_received = 0;
    const auto price_sub = strategy_instance->price_channel().subscribe(
            event_consumer,
            [](const auto & vec) {
                EXPECT_EQ(vec.size(), 0);
            },
            [&](auto, const auto &) {
                ++prices_received;
            });

    const std::chrono::milliseconds ts = std::chrono::milliseconds(1000);
    const double price = 10.1;
    MDPriceEvent ev{{ts, price, SignedVolume{0.}}};
    md_gateway.live_prices_channel().push(ev);
    strategy_instance->wait_event_barrier();
    ASSERT_EQ(prices_received, 1);

    ASSERT_EQ(strategy_status, WorkStatus::Live);
    strategy_instance->stop_async();
    ASSERT_EQ(strategy_status, WorkStatus::Live);
    ASSERT_EQ(md_gateway.unsubscribed_count(), 1) << "Must unsubscribe on stop";
    strategy_instance->wait_event_barrier();
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance.reset();
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
}

// strategy starts in stopped state
// MDGW pushes price event
// strategy sends an order on this price
// TRGW pushes trade and THEN order response
// strategy sends TPSL req
// TRGW pushes TPSL response
// TODO TRGW must push TPSL update
// TRGW pushes close trade without order request
// position closes
// stop stragegy
// TODO resurrect
// TEST_F(StrategyInstanceTest, OpenAndClosePos_GetResult_DontCloseTwiceOnStop)
// {
//     ASSERT_EQ(strategy_status, WorkStatus::Stopped);
//     strategy_instance->run_async();
//     strategy_instance->wait_event_barrier();
//     ASSERT_EQ(md_gateway.live_requests_count(), 1);
//     ASSERT_EQ(strategy_status, WorkStatus::Live);
//     const auto live_req = md_gateway.m_last_live_request.value();
//
//     size_t prices_received = 0;
//     const auto price_sub = strategy_instance->price_channel().subscribe(
//             event_consumer,
//             [](const auto & vec) {
//                 EXPECT_EQ(vec.size(), 0);
//             },
//             [&](auto, const auto &) {
//                 ++prices_received;
//             });
//
//     strategy_ptr->signal_on_next_tick(Side::buy());
//     {
//         const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
//         const double price = 10.1;
//         MDPriceEvent price_event{{price_ts, price, SignedVolume{0.}}};
//         md_gateway.live_prices_channel().push(price_event);
//         strategy_instance->wait_event_barrier();
//         ASSERT_EQ(prices_received, 1);
//     }
//
//     StrategyResult result = strategy_instance->strategy_result_channel().get();
//     ASSERT_EQ(result.trades_count, 0);
//     const auto strategy_res_sub = strategy_instance->strategy_result_channel().subscribe(
//             event_consumer,
//             [&](const auto & res) {
//                 result = res;
//             });
//
//     // opening position
//     ////////////////////////////////////////////////////////////////////////////////////////////////////
//     ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
//     const auto order_req = tr_gateway.m_last_order_request.value();
//     const auto open_trade_price = order_req.order.price();
//     const auto open_trade_volume = order_req.order.target_volume();
//     const auto open_trade_side = order_req.order.side();
//     const std::chrono::milliseconds open_trade_ts = std::chrono::milliseconds(1001);
//     const auto open_trade = Trade{
//             open_trade_ts,
//             m_symbol.symbol_name,
//             {},
//             open_trade_price,
//             open_trade_volume,
//             open_trade_side,
//             0.1};
//     const auto open_trade_event = TradeEvent(open_trade);
//
//     const auto order_response = OrderResponseEvent{
//             order_req.order.symbol(),
//             order_req.order.guid(),
//     };
//
//     ASSERT_FALSE(tr_gateway.m_last_tpsl_request.has_value()) << "Tpsl request must be after position opened";
//     ASSERT_EQ(result.trades_count, 0);
//     tr_gateway.m_trade_channel.push(open_trade_event);
//     tr_gateway.order_response_channel().push(order_response);
//     strategy_instance->wait_event_barrier();
//     ASSERT_EQ(result.trades_count, 1);
//     ////////////////////////////////////////////////////////////////////////////////////////////////////
//
//     {
//         ASSERT_TRUE(tr_gateway.m_last_tpsl_request.has_value());
//         const auto tpsl_req = tr_gateway.m_last_tpsl_request.value();
//         tr_gateway.tpsl_response_channel().push(TpslResponseEvent{
//                 tpsl_req.symbol.symbol_name,
//                 tpsl_req.guid,
//                 tpsl_req.tpsl});
//         // TODO here should be TpslUpdateEvent
//         strategy_instance->wait_event_barrier();
//     }
//
//     // closing position
//     {
//         const auto close_trade_ts = std::chrono::milliseconds(1002);
//         const auto close_trade = Trade{
//                 close_trade_ts,
//                 m_symbol.symbol_name,
//                 {},
//                 open_trade_price * 2,
//                 open_trade_volume,
//                 open_trade_side.opposite(),
//                 0.1};
//         const auto close_trade_event = TradeEvent(close_trade);
//         tr_gateway.m_trade_channel.push(close_trade_event);
//         strategy_instance->wait_event_barrier();
//         ASSERT_EQ(result.trades_count, 2);
//     }
//     ASSERT_EQ(result.win_rate(), 1.) << "It must be a profit position";
//     ASSERT_DOUBLE_EQ(result.final_profit, 99.79) << "It must be a profit position";
//
//     strategy_instance->stop_async();
//     strategy_instance->wait_event_barrier();
//     strategy_instance.reset();
//     ASSERT_EQ(strategy_status, WorkStatus::Stopped);
//     ASSERT_DOUBLE_EQ(result.final_profit, 99.79);
//     ASSERT_EQ(result.trades_count, 2);
// }

// strategy starts in stopped state
// MDGW pushes price event
// strategy sends an order on this price
// TRGW pushes trade and THEN order response
// strategy sends TPSL req
// TRGW pushes TPSL response
// TODO TRGW must push TPSL update
// stop stragegy
// strategy don't stop right now
// strategy sends close order req
// TRGW pushes close trade and THEN order ack
// strategy don't receive TPSL on close
// stragety stops
// TODO resurrect
// TEST_F(StrategyInstanceTest, OpenPositionWithTpsl_CloseOnGracefullStop)
// {
//     ASSERT_EQ(strategy_status, WorkStatus::Stopped);
//     strategy_instance->run_async();
//     strategy_instance->wait_event_barrier();
//     ASSERT_EQ(md_gateway.live_requests_count(), 1);
//     ASSERT_EQ(strategy_status, WorkStatus::Live);
//     const auto live_req = md_gateway.m_last_live_request.value();
//
//     size_t prices_received = 0;
//     const auto price_sub = strategy_instance->price_channel().subscribe(
//             event_consumer,
//             [](const auto & vec) {
//                 EXPECT_EQ(vec.size(), 0);
//             },
//             [&](auto, const auto &) {
//                 ++prices_received;
//             });
//
//     strategy_ptr->signal_on_next_tick(Side::buy());
//     {
//         const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
//         const double price = 10.1;
//         MDPriceEvent price_event{{price_ts, price, SignedVolume{0.}}};
//         md_gateway.live_prices_channel().push(price_event);
//
//         strategy_instance->wait_event_barrier();
//         ASSERT_EQ(prices_received, 1);
//     }
//
//     StrategyResult result = strategy_instance->strategy_result_channel().get();
//     ASSERT_EQ(result.trades_count, 0);
//     const auto strategy_res_sub = strategy_instance->strategy_result_channel().subscribe(
//             event_consumer,
//             [&](const auto & res) {
//                 result = res;
//             });
//
//     // opening position
//     {
//         ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
//         const auto order_req = tr_gateway.m_last_order_request.value();
//         const std::chrono::milliseconds open_trade_ts = std::chrono::milliseconds(1001);
//         const auto open_trade = Trade{
//                 open_trade_ts,
//                 m_symbol.symbol_name,
//                 {},
//                 order_req.order.price(),
//                 order_req.order.target_volume(),
//                 order_req.order.side(),
//                 0.1};
//         const auto open_trade_event = TradeEvent(open_trade);
//
//         const auto order_response = OrderResponseEvent{
//                 order_req.order.symbol(),
//                 order_req.order.guid(),
//         };
//
//         ASSERT_FALSE(tr_gateway.m_last_tpsl_request.has_value()) << "Tpsl request must be after position opened";
//         ASSERT_EQ(result.trades_count, 0);
//         tr_gateway.m_trade_channel.push(open_trade_event);
//         tr_gateway.order_response_channel().push(order_response);
//         strategy_instance->wait_event_barrier();
//         ASSERT_EQ(result.trades_count, 1);
//     }
//
//     {
//         ASSERT_TRUE(tr_gateway.m_last_tpsl_request.has_value());
//         const auto tpsl_req = tr_gateway.m_last_tpsl_request.value();
//         tr_gateway.tpsl_response_channel().push(TpslResponseEvent{
//                 tpsl_req.symbol.symbol_name,
//                 tpsl_req.guid,
//                 tpsl_req.tpsl});
//         // TODO here should be TpslUpdateEvent
//         strategy_instance->wait_event_barrier();
//     }
//
//     strategy_instance->stop_async();
//     strategy_instance->wait_event_barrier();
//     ASSERT_EQ(strategy_status, WorkStatus::Live) << "It won't stop right away";
//     ASSERT_DOUBLE_EQ(result.final_profit, 0.);
//     ASSERT_EQ(result.trades_count, 1) << "Closing trade is not happened yet";
//
//     {
//         ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
//         const auto order_req = tr_gateway.m_last_order_request.value();
//         const std::chrono::milliseconds close_trade_ts = std::chrono::milliseconds(1002);
//         const auto close_trade = Trade{
//                 close_trade_ts,
//                 m_symbol.symbol_name,
//                 {},
//                 order_req.order.price(),
//                 order_req.order.target_volume(),
//                 order_req.order.side(),
//                 0.1};
//         const auto close_trade_event = TradeEvent(close_trade);
//
//         const auto order_response = OrderResponseEvent{
//                 order_req.order.symbol(),
//                 order_req.order.guid(),
//         };
//
//         tr_gateway.m_last_tpsl_request.reset();
//         ASSERT_EQ(result.trades_count, 1);
//         tr_gateway.m_trade_channel.push(close_trade_event);
//         tr_gateway.order_response_channel().push(order_response);
//         strategy_instance->wait_event_barrier();
//         ASSERT_FALSE(tr_gateway.m_last_tpsl_request.has_value()) << "No tpsl on position close";
//         ASSERT_EQ(result.trades_count, 2);
//     }
//
//     ASSERT_EQ(strategy_status, WorkStatus::Stopped);
//     ASSERT_EQ(result.fees_paid, 0.2);
//     ASSERT_DOUBLE_EQ(result.final_profit, -result.fees_paid) << "There was no price move, so we lost only fees";
//     ASSERT_EQ(result.trades_count, 2);
//     strategy_instance.reset();
//     ASSERT_EQ(result.fees_paid, 0.2);
//     ASSERT_DOUBLE_EQ(result.final_profit, -result.fees_paid);
//     ASSERT_EQ(result.trades_count, 2);
// }

TEST_F(StrategyInstanceTest, ManyPricesReceivedWhileOrderIsPending_NoAdditionalOrders)
{
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance->run_async();
    strategy_instance->wait_event_barrier();
    ASSERT_EQ(md_gateway.live_requests_count(), 1);
    ASSERT_EQ(strategy_status, WorkStatus::Live);
    const auto live_req = md_gateway.m_last_live_request.value();

    size_t prices_received = 0;
    const auto price_sub = strategy_instance->price_channel().subscribe(
            event_consumer,
            [](const auto & vec) {
                EXPECT_EQ(vec.size(), 0);
            },
            [&](auto, const auto &) {
                ++prices_received;
            });

    // sending first price
    strategy_ptr->signal_on_next_tick(Side::buy());
    {
        const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
        const double price = 10.1;
        MDPriceEvent price_event{{price_ts, price, SignedVolume{0.}}};
        md_gateway.live_prices_channel().push(price_event);
        strategy_instance->wait_event_barrier();
        ASSERT_EQ(prices_received, 1);
    }

    ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
    const auto order_req = tr_gateway.m_last_order_request.value();

    // sending second price
    strategy_ptr->signal_on_next_tick(Side::buy());
    {
        const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1001);
        const double price = 12.2;
        MDPriceEvent price_event{{price_ts, price, SignedVolume{0.}}};
        md_gateway.live_prices_channel().push(price_event);
        strategy_instance->wait_event_barrier();
        ASSERT_EQ(prices_received, 2);
    }

    ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
    const auto possible_extra_order_req = tr_gateway.m_last_order_request.value();

    // last order request must not change
    ASSERT_EQ(possible_extra_order_req.order.guid(), order_req.order.guid());
    ASSERT_EQ(possible_extra_order_req.order.price(), order_req.order.price());
}

// TODO uncomment after implementing it in OrderManager
// TEST_F(StrategyInstanceTest, EnterOrder_GetReject_Panic)
// {
//     ASSERT_EQ(strategy_status, WorkStatus::Stopped);
//     strategy_instance->run_async();
//     strategy_instance->wait_event_barrier();
//     ASSERT_EQ(md_gateway.live_requests_count(), 1);
//     ASSERT_EQ(strategy_status, WorkStatus::Live);
//
//     strategy_ptr->signal_on_next_tick(Side::buy());
//     {
//         const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
//         const double price = 10.1;
//         MDPriceEvent price_event{{price_ts, price, SignedVolume{0.}}};
//         md_gateway.live_prices_channel().push(price_event);
//         strategy_instance->wait_event_barrier();
//     }
//
//     StrategyResult result = strategy_instance->strategy_result_channel().get();
//     ASSERT_EQ(result.trades_count, 0);
//     const auto strategy_res_sub = strategy_instance->strategy_result_channel().subscribe(
//             event_consumer,
//             [&](const auto & res) {
//                 result = res;
//             });
//
//     ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
//     const auto order_req = tr_gateway.m_last_order_request.value();
//     const auto order_response = OrderResponseEvent{
//             order_req.order.symbol(),
//             order_req.order.guid(),
//             "test_reject"};
//     tr_gateway.order_response_channel().push(order_response);
//     strategy_instance->wait_event_barrier();
//
//     ASSERT_EQ(strategy_status, WorkStatus::Panic);
//     ASSERT_EQ(md_gateway.unsubscribed_count(), 1) << "Must unsubscribe on panic";
// }

// TODO resurrect
// TEST_F(StrategyInstanceTest, OpenPos_TpslReject_ClosePosAndPanic)
// {
//     ASSERT_EQ(strategy_status, WorkStatus::Stopped);
//     strategy_instance->run_async();
//     strategy_instance->wait_event_barrier();
//     ASSERT_EQ(md_gateway.live_requests_count(), 1);
//     ASSERT_EQ(strategy_status, WorkStatus::Live);
//     const auto live_req = md_gateway.m_last_live_request.value();
//
//     strategy_ptr->signal_on_next_tick(Side::buy());
//     {
//         const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
//         const double price = 10.1;
//         MDPriceEvent price_event{{price_ts, price, SignedVolume{0.}}};
//         md_gateway.live_prices_channel().push(price_event);
//         strategy_instance->wait_event_barrier();
//     }
//
//     StrategyResult result = strategy_instance->strategy_result_channel().get();
//     ASSERT_EQ(result.trades_count, 0);
//     const auto strategy_res_sub = strategy_instance->strategy_result_channel().subscribe(
//             event_consumer,
//             [&](const auto & res) {
//                 result = res;
//             });
//
//     {
//         ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
//         const auto order_req = tr_gateway.m_last_order_request.value();
//         const auto open_trade_price = order_req.order.price();
//         const auto open_trade_volume = order_req.order.target_volume();
//         const auto open_trade_side = order_req.order.side();
//         const std::chrono::milliseconds open_trade_ts = std::chrono::milliseconds(1001);
//         const auto open_trade = Trade{
//                 open_trade_ts,
//                 m_symbol.symbol_name,
//                 {},
//                 open_trade_price,
//                 open_trade_volume,
//                 open_trade_side,
//                 0.1};
//         const auto open_trade_event = TradeEvent(open_trade);
//
//         const auto order_response = OrderResponseEvent{
//                 order_req.order.symbol(),
//                 order_req.order.guid(),
//         };
//
//         ASSERT_FALSE(tr_gateway.m_last_tpsl_request.has_value()) << "Tpsl request must be after position opened";
//         ASSERT_EQ(result.trades_count, 0);
//         tr_gateway.m_trade_channel.push(open_trade_event);
//         tr_gateway.order_response_channel().push(order_response);
//         strategy_instance->wait_event_barrier();
//         ASSERT_EQ(result.trades_count, 1);
//     }
//
//     {
//         ASSERT_TRUE(tr_gateway.m_last_tpsl_request.has_value());
//         const auto tpsl_req = tr_gateway.m_last_tpsl_request.value();
//         tr_gateway.tpsl_response_channel().push(TpslResponseEvent{
//                 tpsl_req.symbol.symbol_name,
//                 tpsl_req.guid,
//                 tpsl_req.tpsl,
//                 "test_reject"});
//         strategy_instance->wait_event_barrier();
//     }
//
//     // There must be a close pos request on panic
//     {
//         ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
//         const auto order_req = tr_gateway.m_last_order_request.value();
//         const auto close_trade_price = order_req.order.price();
//         const auto close_trade_volume = order_req.order.target_volume();
//         const auto close_trade_side = order_req.order.side();
//         const std::chrono::milliseconds close_trade_ts = std::chrono::milliseconds(1001);
//         const auto close_trade = Trade{
//                 close_trade_ts,
//                 m_symbol.symbol_name,
//                 {},
//                 close_trade_price,
//                 close_trade_volume,
//                 close_trade_side,
//                 0.1};
//         const auto close_trade_event = TradeEvent(close_trade);
//
//         const auto order_response = OrderResponseEvent{
//                 order_req.order.symbol(),
//                 order_req.order.guid(),
//         };
//
//         ASSERT_EQ(result.trades_count, 1);
//         tr_gateway.m_trade_channel.push(close_trade_event);
//         tr_gateway.order_response_channel().push(order_response);
//         strategy_instance->wait_event_barrier();
//         ASSERT_EQ(result.trades_count, 2);
//     }
//
//     ASSERT_EQ(strategy_status, WorkStatus::Panic);
//     ASSERT_EQ(md_gateway.unsubscribed_count(), 1) << "Must unsubscribe on panic";
//     ASSERT_EQ(result.trades_count, 2);
// }

// TODO
// TEST_F(StrategyInstanceTest, PanicOnMarketDataStop) {}
// TEST_F(StrategyInstanceTest, PanicOnTradingStop) {}

} // namespace test
