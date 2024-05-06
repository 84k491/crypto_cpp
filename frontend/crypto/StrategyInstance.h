#pragma once

#include "EventLoop.h"
#include "IMarketDataGateway.h"
#include "ITradingGateway.h"
#include "ObjectPublisher.h"
#include "PositionManager.h"
#include "Signal.h"
#include "StrategyInterface.h"
#include "StrategyResult.h"
#include "TimeseriesPublisher.h"
#include "TpslExitStrategy.h"
#include "WorkStatus.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <set>
#include <variant>

#define STRATEGY_EVENTS HistoricalMDPackEvent, \
                        MDPriceEvent,          \
                        OrderResponseEvent,    \
                        TradeEvent,            \
                        TpslResponseEvent,     \
                        TpslUpdatedEvent,      \
                        StrategyStopRequest

struct StrategyStopRequest : public BasicResponseEvent
{
};

class StrategyInstance : public IEventInvoker<STRATEGY_EVENTS>
{
public:
    using KlineCallback =
            std::function<void(std::pair<std::chrono::milliseconds, OHLC>)>;
    using DepoCallback =
            std::function<void(std::chrono::milliseconds ts, double value)>;

    StrategyInstance(
            const Symbol & symbol,
            const std::optional<HistoricalMDRequestData> & historical_md_request,
            const std::shared_ptr<IStrategy> & strategy_ptr,
            TpslExitStrategyConfig exit_strategy_config,
            IMarketDataGateway & md_gateway,
            ITradingGateway & tr_gateway);

    ~StrategyInstance() override;

    TimeseriesPublisher<Signal> & signals_publisher();
    TimeseriesPublisher<std::pair<std::string, double>> &
    strategy_internal_data_publisher();
    TimeseriesPublisher<OHLC> & klines_publisher();
    TimeseriesPublisher<double> & depo_publisher();
    ObjectPublisher<StrategyResult> & strategy_result_publisher();
    ObjectPublisher<WorkStatus> & status_publisher();
    TimeseriesPublisher<Tpsl> & tpsl_publisher();

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
    static void handle_event(const TpslUpdatedEvent & response);
    void handle_event(const StrategyStopRequest & response);

    void on_signal(const Signal & signal);
    void process_position_result(const PositionResult & new_result,
                                 std::chrono::milliseconds ts);

    bool open_position(double price, SignedVolume target_absolute_volume, std::chrono::milliseconds ts);
    bool close_position(double price, std::chrono::milliseconds ts);
    void set_tpsl(Tpsl tpsl);

    bool ready_to_finish() const;
    void finish_if_needed_and_ready();

private:
    xg::Guid m_strategy_guid;

    EventLoop<STRATEGY_EVENTS> m_event_loop;

    IMarketDataGateway & m_md_gateway;
    ITradingGateway & m_tr_gateway;
    std::shared_ptr<IStrategy> m_strategy;
    TpslExitStrategy m_exit_strategy;

    ObjectPublisher<StrategyResult> m_strategy_result;

    TimeseriesPublisher<Signal> m_signal_publisher; // TODO publish trades instead of signals
    TimeseriesPublisher<OHLC> m_klines_publisher;
    TimeseriesPublisher<double> m_depo_publisher;
    TimeseriesPublisher<Tpsl> m_tpsl_publisher;

    const Symbol m_symbol;
    static constexpr double m_pos_currency_amount = 100.;
    PositionManager m_position_manager;

    const std::optional<HistoricalMDRequestData> m_historical_md_request;

    std::shared_ptr<ObjectSubscribtion<WorkStatus>> m_gw_status_sub;
    bool first_price_received = false;

    std::pair<std::chrono::milliseconds, double> m_last_ts_and_price;

    ObjectPublisher<WorkStatus> m_status;

    std::set<xg::Guid> m_live_md_requests;
    std::set<xg::Guid> m_pending_requests;
    std::set<xg::Guid> m_pending_orders;

    bool m_stop_request_handled = false;
    WorkStatus m_status_on_stop = WorkStatus::Stopped;
    std::optional<std::promise<void>> m_finish_promise;

    bool m_backtest_in_progress = false;
};
