#pragma once

#include "EventLoop.h"
#include "EventTimeseriesPublisher.h"
#include "ExitStrategyInterface.h"
#include "IMarketDataGateway.h"
#include "ITradingGateway.h"
#include "JsonStrategyConfig.h"
#include "PositionManager.h"
#include "Signal.h"
#include "StrategyInterface.h"
#include "StrategyResult.h"
#include "WorkStatus.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <set>
#include <variant>

class StrategyInstance : public IEventInvoker<STRATEGY_EVENTS>
{
public:
    StrategyInstance(
            const Symbol & symbol,
            const std::optional<HistoricalMDRequestData> & historical_md_request,
            const std::shared_ptr<IStrategy> & strategy_ptr,
            const std::string & exit_strategy_name,
            const JsonStrategyConfig & exit_strategy_config,
            IMarketDataGateway & md_gateway,
            ITradingGateway & tr_gateway);

    ~StrategyInstance() override;

    EventTimeseriesPublisher<Trade> & trade_publisher();
    EventTimeseriesPublisher<std::tuple<std::string, std::string, double>> &
    strategy_internal_data_publisher();
    EventTimeseriesPublisher<OHLC> & klines_publisher();
    EventTimeseriesPublisher<double> & depo_publisher();
    EventObjectPublisher<StrategyResult> & strategy_result_publisher();
    EventObjectPublisher<WorkStatus> & status_publisher();
    EventTimeseriesPublisher<Tpsl> & tpsl_publisher();
    EventTimeseriesPublisher<StopLoss> & trailing_stop_publisher();

    void run_async();
    void stop_async(bool panic = false);
    [[nodiscard("wait in future")]] std::future<void> wait_for_finish();

private:
    void invoke(const std::variant<STRATEGY_EVENTS> & value) override;
    void handle_event(const HistoricalMDPackEvent & response);
    void handle_event(const MDPriceEvent & response);
    void handle_event(const OrderResponseEvent & response);
    void handle_event(const TradeEvent & response);
    void handle_event(const TpslResponseEvent & response);
    void handle_event(const TpslUpdatedEvent & response);
    void handle_event(const StrategyStopRequest & response);
    void handle_event(const TrailingStopLossResponseEvent & response);
    void handle_event(const TrailingStopLossUpdatedEvent & response);
    static void handle_event(const LambdaEvent & response);

    void on_signal(const Signal & signal);
    void process_position_result(const PositionResult & new_result,
                                 std::chrono::milliseconds ts);

    bool open_position(double price, SignedVolume target_absolute_volume, std::chrono::milliseconds ts);
    bool close_position(double price, std::chrono::milliseconds ts);

    bool ready_to_finish() const;
    void finish_if_needed_and_ready();

private:
    const xg::Guid m_strategy_guid;

    IMarketDataGateway & m_md_gateway;
    ITradingGateway & m_tr_gateway;
    std::shared_ptr<IStrategy> m_strategy;

    EventObjectPublisher<StrategyResult> m_strategy_result;

    EventTimeseriesPublisher<Trade> m_trade_publisher;
    EventTimeseriesPublisher<OHLC> m_klines_publisher;
    EventTimeseriesPublisher<double> m_depo_publisher;

    const Symbol m_symbol;
    static constexpr double m_pos_currency_amount = 100.;
    PositionManager m_position_manager;
    std::shared_ptr<IExitStrategy> m_exit_strategy;

    const std::optional<HistoricalMDRequestData> m_historical_md_request;

    std::shared_ptr<EventObjectSubscription<WorkStatus>> m_gw_status_sub;
    bool first_price_received = false;

    std::pair<std::chrono::milliseconds, double> m_last_ts_and_price;

    EventObjectPublisher<WorkStatus> m_status;

    std::set<xg::Guid> m_live_md_requests;
    std::set<xg::Guid> m_pending_requests;
    std::map<xg::Guid, MarketOrder> m_pending_orders;

    bool m_stop_request_handled = false;
    WorkStatus m_status_on_stop = WorkStatus::Stopped;
    std::optional<std::promise<void>> m_finish_promise;

    bool m_backtest_in_progress = false;

    EventLoopHolder<STRATEGY_EVENTS> m_event_loop;
    std::list<std::shared_ptr<ISubsription>> m_subscriptions; // those must be destroyed before EvLoop
};
