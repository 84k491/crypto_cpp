#include "StrategyInstance.h"

#include "ScopeExit.h"
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

    void push_async_request(HistoricalMDRequest && request) override
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

    ObjectPublisher<WorkStatus> & status_publisher() override
    {
        return m_status;
    }

    size_t live_requests_count() const { return m_live_requests_count; }
    size_t unsubscribed_count() const { return m_unsubscribed_count; }

public:
    std::optional<LiveMDRequest> m_last_live_request;

private:
    ObjectPublisher<WorkStatus> m_status;

    size_t m_live_requests_count = 0;
    size_t m_unsubscribed_count = 0;
};

class MockTradingGateway : public ITradingGateway
{
public:
    ~MockTradingGateway() override = default;

    void push_order_request(const OrderRequestEvent & order) override
    {
    }
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override
    {
    }

    void register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers) override
    {
    }
    void unregister_consumers(xg::Guid guid) override
    {
    }
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

    TimeseriesPublisher<std::pair<std::string, double>> & strategy_internal_data_publisher() override
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
    TimeseriesPublisher<std::pair<std::string, double>> m_strategy_internal_data_publisher;

    std::optional<Side> m_next_signal_side;
};

// TODO don't use sleeps
class StrategyInstanceTest : public Test
{
public:
    StrategyInstanceTest()
        : m_symbol("BTCUSD")
    {
    }

protected:
    Symbol m_symbol;
};

TEST_F(StrategyInstanceTest, SubForLiveMarketData_GetPrice_GracefullStop)
{
    MockMDGateway md_gateway;
    MockTradingGateway tr_gateway;

    std::shared_ptr<MockStrategy> strategy_ptr = std::make_shared<MockStrategy>();
    TpslExitStrategyConfig exit_strategy_config(0.1, 0.8);
    auto strategy_instance = std::make_unique<StrategyInstance>(
            m_symbol,
            std::nullopt,
            strategy_ptr,
            exit_strategy_config,
            md_gateway,
            tr_gateway);

    WorkStatus strategy_status = strategy_instance->status_publisher().get();
    const auto status_sub = strategy_instance->status_publisher().subscribe([&](const auto & status) {
        strategy_status = status;
    });

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

TEST_F(StrategyInstanceTest, OpenPositionWithTpsl_CloseOnGracefullStop) {}
TEST_F(StrategyInstanceTest, OpenAndClosePos_GetResult_DontCloseOnStop) {}
TEST_F(StrategyInstanceTest, OpenPos_CloseTradeFromWeb_PosClosed) {}
TEST_F(StrategyInstanceTest, EnterOrder_GetReject_Panic) {}
TEST_F(StrategyInstanceTest, OpenPos_TpslReject_ClosePosAndPanic) {}
TEST_F(StrategyInstanceTest, ManyPricesReceivedBetweenEnterOrderAndOpenTrade_NoAdditionalOrders) {}

} // namespace test
