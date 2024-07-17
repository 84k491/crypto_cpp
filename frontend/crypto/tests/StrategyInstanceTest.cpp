#include "StrategyInstance.h"

#include "ScopeExit.h"
#include "Trade.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <print>

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

    void push_async_request(LiveMDRequest && request) override
    {
        ++m_live_requests_count;
        m_last_live_request = std::move(request);
    }

    void unsubscribe_from_live(xg::Guid) override
    {
        ++m_unsubscribed_count;
    }

    EventObjectPublisher<WorkStatus> & status_publisher() override
    {
        return m_status;
    }

    size_t live_requests_count() const { return m_live_requests_count; }
    size_t unsubscribed_count() const { return m_unsubscribed_count; }

public:
    std::optional<LiveMDRequest> m_last_live_request;

private:
    EventObjectPublisher<WorkStatus> m_status;

    size_t m_live_requests_count = 0;
    size_t m_unsubscribed_count = 0;
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

    void register_consumers(xg::Guid, const Symbol &, TradingGatewayConsumers consumers) override
    {
        m_consumers = std::make_unique<TradingGatewayConsumers>(consumers);
    }

    void unregister_consumers(xg::Guid) override
    {
        m_consumers.reset();
    }

public:
    std::optional<OrderRequestEvent> m_last_order_request;
    std::optional<TpslRequestEvent> m_last_tpsl_request;

    std::unique_ptr<TradingGatewayConsumers> m_consumers;
};

class MockStrategy : public IStrategy
{
public:
    ~MockStrategy() override = default;

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override
    {
        ScopeExit se([&] { m_next_signal_side = std::nullopt; });
        if (m_next_signal_side.has_value()) {
            Signal result = {.side = *m_next_signal_side, .timestamp = ts_and_price.first, .price = ts_and_price.second};
            return result;
        }
        return std::nullopt;
    }

    EventTimeseriesPublisher<std::pair<std::string, double>> & strategy_internal_data_publisher() override
    {
        return m_strategy_internal_data_publisher;
    }

    bool is_valid() const override { return true; }

public:
    void signal_on_next_tick(const Side & signal_side)
    {
        m_next_signal_side = signal_side;
    }

private:
    EventTimeseriesPublisher<std::pair<std::string, double>> m_strategy_internal_data_publisher;

    std::optional<Side> m_next_signal_side;
};

// TODO don't use sleeps
class StrategyInstanceTest
    : public Test
    , public IEventConsumer<LambdaEvent>
{
public:
    StrategyInstanceTest()
        : m_symbol("BTCUSD")
        , strategy_ptr(std::make_shared<MockStrategy>())
        , exit_strategy_config(0.1, 0.8)
        , strategy_instance(nullptr)
    {
        m_symbol.lot_size_filter.max_qty = 1'000'000;
        m_symbol.lot_size_filter.min_qty = 0.001;
        m_symbol.lot_size_filter.qty_step = 0.001;

        strategy_instance = std::make_unique<StrategyInstance>(
                m_symbol,
                std::nullopt,
                strategy_ptr,
                exit_strategy_config,
                md_gateway,
                tr_gateway);

        {
            strategy_status = strategy_instance->status_publisher().get();
        }

        status_sub = strategy_instance->status_publisher().subscribe(
                *this,
                [&](const auto & status) {
                    strategy_status = status;
                });
    }

    bool push_to_queue(std::any value) override
    {
        auto & lambda_event = std::any_cast<LambdaEvent &>(value);
        lambda_event.func();
        return true;
    }

    bool push_to_queue_delayed(std::chrono::milliseconds, const std::any) override
    {
        throw std::runtime_error("Not implemented");
    }

protected:
    Symbol m_symbol;

    MockMDGateway md_gateway;
    MockTradingGateway tr_gateway;

    std::shared_ptr<MockStrategy> strategy_ptr = std::make_shared<MockStrategy>();
    TpslExitStrategyConfig exit_strategy_config;
    std::unique_ptr<StrategyInstance> strategy_instance;

    WorkStatus strategy_status = WorkStatus::Panic;
    std::shared_ptr<EventObjectSubscribtion<WorkStatus>> status_sub;
};

TEST_F(StrategyInstanceTest, SubForLiveMarketData_GetPrice_GracefullStop)
{
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance->run_async();
    ASSERT_EQ(md_gateway.live_requests_count(), 1);
    ASSERT_EQ(strategy_status, WorkStatus::Live);

    const auto live_req = md_gateway.m_last_live_request.value();

    size_t prices_received = 0;
    const auto price_sub = strategy_instance->klines_publisher().subscribe(
            [](const auto & vec) {
                EXPECT_EQ(vec.size(), 0);
            },
            [&](auto, const auto &) {
                ++prices_received;
            });

    const std::chrono::milliseconds ts = std::chrono::milliseconds(1000);
    const double price = 10.1;
    OHLC ohlc = {ts, price, price, price, price};
    MDPriceEvent ev;
    ev.ts_and_price = {ts, ohlc};
    live_req.event_consumer->push(ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(prices_received, 1);

    ASSERT_EQ(strategy_status, WorkStatus::Live);
    strategy_instance->stop_async();
    ASSERT_EQ(strategy_status, WorkStatus::Live);
    ASSERT_EQ(md_gateway.unsubscribed_count(), 1) << "Must unsubscribe on stop";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance.reset();
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
}

TEST_F(StrategyInstanceTest, OpenAndClosePos_GetResult_DontCloseTwiceOnStop)
{
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance->run_async();
    ASSERT_EQ(md_gateway.live_requests_count(), 1);
    ASSERT_EQ(strategy_status, WorkStatus::Live);
    const auto live_req = md_gateway.m_last_live_request.value();

    size_t prices_received = 0;
    const auto price_sub = strategy_instance->klines_publisher().subscribe(
            [](const auto & vec) {
                EXPECT_EQ(vec.size(), 0);
            },
            [&](auto, const auto &) {
                ++prices_received;
            });

    ASSERT_TRUE(tr_gateway.m_consumers);
    strategy_ptr->signal_on_next_tick(Side::Buy);
    {
        const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
        const double price = 10.1;
        OHLC ohlc = {price_ts, price, price, price, price};
        MDPriceEvent price_event;
        price_event.ts_and_price = {price_ts, ohlc};
        live_req.event_consumer->push(price_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(prices_received, 1);
    }

    StrategyResult result = strategy_instance->strategy_result_publisher().get();
    ASSERT_EQ(result.trades_count, 0);
    const auto strategy_res_sub = strategy_instance->strategy_result_publisher().subscribe(
            *this,
            [&](const auto & res) {
                result = res;
            });

    // opening position
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
    const auto order_req = tr_gateway.m_last_order_request.value();
    const auto open_trade_price = order_req.order.price();
    const auto open_trade_volume = order_req.order.volume();
    const auto open_trade_side = order_req.order.side();
    const std::chrono::milliseconds open_trade_ts = std::chrono::milliseconds(1001);
    const auto open_trade = Trade{
            open_trade_ts,
            m_symbol.symbol_name,
            open_trade_price,
            open_trade_volume,
            open_trade_side,
            0.1};
    const auto open_trade_event = TradeEvent(open_trade);

    const auto order_response = OrderResponseEvent{
            order_req.order.guid(),
    };

    ASSERT_FALSE(tr_gateway.m_last_tpsl_request.has_value()) << "Tpsl request must be after position opened";
    ASSERT_EQ(result.trades_count, 0);
    tr_gateway.m_consumers->trade_consumer.push(open_trade_event);
    tr_gateway.m_consumers->order_ack_consumer.push(order_response);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(result.trades_count, 1);
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    {
        ASSERT_TRUE(tr_gateway.m_last_tpsl_request.has_value());
        const auto tpsl_req = tr_gateway.m_last_tpsl_request.value();
        tpsl_req.event_consumer->push(TpslResponseEvent{tpsl_req.guid, tpsl_req.tpsl});
        // TODO here should be TpslUpdateEvent
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // closing position
    {
        const auto close_trade_ts = std::chrono::milliseconds(1002);
        const auto close_trade = Trade{
                close_trade_ts,
                m_symbol.symbol_name,
                open_trade_price * 2,
                open_trade_volume,
                open_trade_side == Side::Buy ? Side::Sell : Side::Buy,
                0.1};
        const auto close_trade_event = TradeEvent(close_trade);
        tr_gateway.m_consumers->trade_consumer.push(close_trade_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(result.trades_count, 2);
    }
    ASSERT_EQ(result.win_rate(), 1.) << "It must be a profit position";
    ASSERT_DOUBLE_EQ(result.final_profit, 99.79) << "It must be a profit position";

    strategy_instance->stop_async();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    strategy_instance.reset();
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    ASSERT_DOUBLE_EQ(result.final_profit, 99.79);
    ASSERT_EQ(result.trades_count, 2);
}

TEST_F(StrategyInstanceTest, OpenPositionWithTpsl_CloseOnGracefullStop)
{
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance->run_async();
    ASSERT_EQ(md_gateway.live_requests_count(), 1);
    ASSERT_EQ(strategy_status, WorkStatus::Live);
    const auto live_req = md_gateway.m_last_live_request.value();

    size_t prices_received = 0;
    const auto price_sub = strategy_instance->klines_publisher().subscribe(
            [](const auto & vec) {
                EXPECT_EQ(vec.size(), 0);
            },
            [&](auto, const auto &) {
                ++prices_received;
            });

    ASSERT_TRUE(tr_gateway.m_consumers);
    strategy_ptr->signal_on_next_tick(Side::Buy);
    {
        const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
        const double price = 10.1;
        OHLC ohlc = {price_ts, price, price, price, price};
        MDPriceEvent price_event;
        price_event.ts_and_price = {price_ts, ohlc};
        live_req.event_consumer->push(price_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(prices_received, 1);
    }

    StrategyResult result = strategy_instance->strategy_result_publisher().get();
    ASSERT_EQ(result.trades_count, 0);
    const auto strategy_res_sub = strategy_instance->strategy_result_publisher().subscribe(
            *this,
            [&](const auto & res) {
                result = res;
            });

    // opening position
    {
        ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
        const auto order_req = tr_gateway.m_last_order_request.value();
        const std::chrono::milliseconds open_trade_ts = std::chrono::milliseconds(1001);
        const auto open_trade = Trade{
                open_trade_ts,
                m_symbol.symbol_name,
                order_req.order.price(),
                order_req.order.volume(),
                order_req.order.side(),
                0.1};
        const auto open_trade_event = TradeEvent(open_trade);

        const auto order_response = OrderResponseEvent{
                order_req.order.guid(),
        };

        ASSERT_FALSE(tr_gateway.m_last_tpsl_request.has_value()) << "Tpsl request must be after position opened";
        ASSERT_EQ(result.trades_count, 0);
        tr_gateway.m_consumers->trade_consumer.push(open_trade_event);
        tr_gateway.m_consumers->order_ack_consumer.push(order_response);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(result.trades_count, 1);
    }

    {
        ASSERT_TRUE(tr_gateway.m_last_tpsl_request.has_value());
        const auto tpsl_req = tr_gateway.m_last_tpsl_request.value();
        tpsl_req.event_consumer->push(TpslResponseEvent{tpsl_req.guid, tpsl_req.tpsl});
        // TODO here should be TpslUpdateEvent
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    strategy_instance->stop_async();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(strategy_status, WorkStatus::Live) << "It won't stop right away";
    ASSERT_DOUBLE_EQ(result.final_profit, 0.);
    ASSERT_EQ(result.trades_count, 1) << "Closing trade is not happened yet";

    {
        ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
        const auto order_req = tr_gateway.m_last_order_request.value();
        const std::chrono::milliseconds close_trade_ts = std::chrono::milliseconds(1002);
        const auto close_trade = Trade{
                close_trade_ts,
                m_symbol.symbol_name,
                order_req.order.price(),
                order_req.order.volume(),
                order_req.order.side(),
                0.1};
        const auto close_trade_event = TradeEvent(close_trade);

        const auto order_response = OrderResponseEvent{
                order_req.order.guid(),
        };

        tr_gateway.m_last_tpsl_request.reset();
        ASSERT_EQ(result.trades_count, 1);
        tr_gateway.m_consumers->trade_consumer.push(close_trade_event);
        tr_gateway.m_consumers->order_ack_consumer.push(order_response);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_FALSE(tr_gateway.m_last_tpsl_request.has_value()) << "No tpsl on position close";
        ASSERT_EQ(result.trades_count, 2);
    }

    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    ASSERT_EQ(result.fees_paid, 0.2);
    ASSERT_DOUBLE_EQ(result.final_profit, -result.fees_paid) << "There was no price move, so we lost only fees";
    ASSERT_EQ(result.trades_count, 2);
    strategy_instance.reset();
    ASSERT_EQ(result.fees_paid, 0.2);
    ASSERT_DOUBLE_EQ(result.final_profit, -result.fees_paid);
    ASSERT_EQ(result.trades_count, 2);
}

TEST_F(StrategyInstanceTest, ManyPricesReceivedWhileOrderIsPending_NoAdditionalOrders)
{
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance->run_async();
    ASSERT_EQ(md_gateway.live_requests_count(), 1);
    ASSERT_EQ(strategy_status, WorkStatus::Live);
    const auto live_req = md_gateway.m_last_live_request.value();

    size_t prices_received = 0;
    const auto price_sub = strategy_instance->klines_publisher().subscribe(
            [](const auto & vec) {
                EXPECT_EQ(vec.size(), 0);
            },
            [&](auto, const auto &) {
                ++prices_received;
            });

    ASSERT_TRUE(tr_gateway.m_consumers);

    // sending first price
    strategy_ptr->signal_on_next_tick(Side::Buy);
    {
        const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
        const double price = 10.1;
        OHLC ohlc = {price_ts, price, price, price, price};
        MDPriceEvent price_event;
        price_event.ts_and_price = {price_ts, ohlc};
        live_req.event_consumer->push(price_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(prices_received, 1);
    }

    ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
    const auto order_req = tr_gateway.m_last_order_request.value();

    // sending second price
    strategy_ptr->signal_on_next_tick(Side::Buy);
    {
        const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1001);
        const double price = 12.2;
        OHLC ohlc = {price_ts, price, price, price, price};
        MDPriceEvent price_event;
        price_event.ts_and_price = {price_ts, ohlc};
        live_req.event_consumer->push(price_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(prices_received, 2);
    }

    ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
    const auto possible_extra_order_req = tr_gateway.m_last_order_request.value();

    // last order request must not change
    ASSERT_EQ(possible_extra_order_req.order.guid(), order_req.order.guid());
    ASSERT_EQ(possible_extra_order_req.order.price(), order_req.order.price());
}

TEST_F(StrategyInstanceTest, EnterOrder_GetReject_Panic)
{
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance->run_async();
    ASSERT_EQ(md_gateway.live_requests_count(), 1);
    ASSERT_EQ(strategy_status, WorkStatus::Live);
    const auto live_req = md_gateway.m_last_live_request.value();

    ASSERT_TRUE(tr_gateway.m_consumers);
    strategy_ptr->signal_on_next_tick(Side::Buy);
    {
        const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
        const double price = 10.1;
        OHLC ohlc = {price_ts, price, price, price, price};
        MDPriceEvent price_event;
        price_event.ts_and_price = {price_ts, ohlc};
        live_req.event_consumer->push(price_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    StrategyResult result = strategy_instance->strategy_result_publisher().get();
    ASSERT_EQ(result.trades_count, 0);
    const auto strategy_res_sub = strategy_instance->strategy_result_publisher().subscribe(
            *this,
            [&](const auto & res) {
                result = res;
            });

    ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
    const auto order_req = tr_gateway.m_last_order_request.value();
    const auto order_response = OrderResponseEvent{
            order_req.order.guid(), "test_reject"};
    order_req.event_consumer->push(order_response);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_EQ(strategy_status, WorkStatus::Panic);
    ASSERT_FALSE(tr_gateway.m_consumers) << "Must unsubscribe on panic";
    ASSERT_EQ(md_gateway.unsubscribed_count(), 1) << "Must unsubscribe on panic";
}

TEST_F(StrategyInstanceTest, OpenPos_TpslReject_ClosePosAndPanic)
{
    ASSERT_EQ(strategy_status, WorkStatus::Stopped);
    strategy_instance->run_async();
    ASSERT_EQ(md_gateway.live_requests_count(), 1);
    ASSERT_EQ(strategy_status, WorkStatus::Live);
    const auto live_req = md_gateway.m_last_live_request.value();

    ASSERT_TRUE(tr_gateway.m_consumers);
    strategy_ptr->signal_on_next_tick(Side::Buy);
    {
        const std::chrono::milliseconds price_ts = std::chrono::milliseconds(1000);
        const double price = 10.1;
        OHLC ohlc = {price_ts, price, price, price, price};
        MDPriceEvent price_event;
        price_event.ts_and_price = {price_ts, ohlc};
        live_req.event_consumer->push(price_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    StrategyResult result = strategy_instance->strategy_result_publisher().get();
    ASSERT_EQ(result.trades_count, 0);
    const auto strategy_res_sub = strategy_instance->strategy_result_publisher().subscribe(
            *this,
            [&](const auto & res) {
                result = res;
            });

    {
        ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
        const auto order_req = tr_gateway.m_last_order_request.value();
        const auto open_trade_price = order_req.order.price();
        const auto open_trade_volume = order_req.order.volume();
        const auto open_trade_side = order_req.order.side();
        const std::chrono::milliseconds open_trade_ts = std::chrono::milliseconds(1001);
        const auto open_trade = Trade{
                open_trade_ts,
                m_symbol.symbol_name,
                open_trade_price,
                open_trade_volume,
                open_trade_side,
                0.1};
        const auto open_trade_event = TradeEvent(open_trade);

        const auto order_response = OrderResponseEvent{
                order_req.order.guid(),
        };

        ASSERT_FALSE(tr_gateway.m_last_tpsl_request.has_value()) << "Tpsl request must be after position opened";
        ASSERT_EQ(result.trades_count, 0);
        tr_gateway.m_consumers->trade_consumer.push(open_trade_event);
        tr_gateway.m_consumers->order_ack_consumer.push(order_response);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(result.trades_count, 1);
    }

    {
        ASSERT_TRUE(tr_gateway.m_last_tpsl_request.has_value());
        const auto tpsl_req = tr_gateway.m_last_tpsl_request.value();
        tpsl_req.event_consumer->push(TpslResponseEvent{tpsl_req.guid, tpsl_req.tpsl, "test_reject"});
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // There must be a close pos request on panic
    {
        ASSERT_TRUE(tr_gateway.m_last_order_request.has_value());
        const auto order_req = tr_gateway.m_last_order_request.value();
        const auto close_trade_price = order_req.order.price();
        const auto close_trade_volume = order_req.order.volume();
        const auto close_trade_side = order_req.order.side();
        const std::chrono::milliseconds close_trade_ts = std::chrono::milliseconds(1001);
        const auto close_trade = Trade{
                close_trade_ts,
                m_symbol.symbol_name,
                close_trade_price,
                close_trade_volume,
                close_trade_side,
                0.1};
        const auto close_trade_event = TradeEvent(close_trade);

        const auto order_response = OrderResponseEvent{
                order_req.order.guid(),
        };

        ASSERT_EQ(result.trades_count, 1);
        tr_gateway.m_consumers->trade_consumer.push(close_trade_event);
        tr_gateway.m_consumers->order_ack_consumer.push(order_response);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(result.trades_count, 2);
    }

    ASSERT_EQ(strategy_status, WorkStatus::Panic);
    ASSERT_FALSE(tr_gateway.m_consumers) << "Must unsubscribe on panic";
    ASSERT_EQ(md_gateway.unsubscribed_count(), 1) << "Must unsubscribe on panic";
    ASSERT_EQ(result.trades_count, 2);
}

// TODO
// TEST_F(StrategyInstanceTest, PanicOnMarketDataStop) {}
// TEST_F(StrategyInstanceTest, PanicOnTradingStop) {}

} // namespace test
