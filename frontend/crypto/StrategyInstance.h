#pragma once

#include "CandleBuiler.h"
#include "ConditionalOrders.h"
#include "EventLoop.h"
#include "EventLoopSubscriber.h"
#include "EventTimeseriesChannel.h"
#include "IMarketDataGateway.h"
#include "ITradingGateway.h"
#include "JsonStrategyConfig.h"
#include "MarketState.h"
#include "OrderManager.h"
#include "PositionManager.h"
#include "StrategyChannels.h"
#include "StrategyInterface.h"
#include "StrategyResult.h"
#include "WorkStatus.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <set>

class StrategyInstance
{
public:
    StrategyInstance(
            const Symbol & symbol,
            const std::optional<HistoricalMDRequestData> & historical_md_request,
            const std::string & entry_strategy_name,
            const JsonStrategyConfig & entry_strategy_config,
            IMarketDataGateway & md_gateway,
            ITradingGateway & tr_gateway);

    ~StrategyInstance();

    void set_channel_capacity(std::optional<std::chrono::milliseconds> capacity);
    EventTimeseriesChannel<Trade> & trade_channel();
    EventTimeseriesChannel<ProfitPriceLevels> & price_levels_channel();
    EventTimeseriesChannel<StrategyInternalData> & strategy_internal_data_channel();
    EventTimeseriesChannel<double> & price_channel();
    EventTimeseriesChannel<Candle> & candle_channel();
    EventTimeseriesChannel<double> & depo_channel();
    EventTimeseriesChannel<PositionResult> & positions_channel();
    EventObjectChannel<StrategyResult> & strategy_result_channel();
    EventObjectChannel<WorkStatus> & status_channel();
    EventTimeseriesChannel<TpslPrices> & tpsl_channel();
    EventTimeseriesChannel<StopLoss> & trailing_stop_channel();
    EventTimeseriesChannel<MarketStateRenderObject> & market_state_channel();

    void run_async();
    void stop_async(bool panic = false);
    [[nodiscard("wait in future")]] std::future<void> finish_future();
    void wait_event_barrier();

    // for tests
    std::shared_ptr<IStrategy> get_strategy() const { return m_strategy; }

private:
    template <class T>
    void handle_event_generic(const T & ev);

    void handle_event(const HistoricalMDGeneratorEvent & response);
    void handle_event(const HistoricalMDPriceEvent & response);
    void handle_event(const MDPriceEvent & response);
    void handle_event(const OrderResponseEvent & response);
    void handle_event(const TradeEvent & response);
    void handle_event(const StrategyStartRequest & ev);
    void handle_event(const StrategyStopRequest & response);
    void after_every_event();

    void process_market_state_update(MarketState state);

    void process_position_result(const PositionResult & new_result,
                                 std::chrono::milliseconds ts);

    void close_position(double price, std::chrono::milliseconds ts);

    bool ready_to_finish() const;
    void finish_if_needed_and_ready();

private:
    EventLoop m_event_loop;

    const xg::Guid m_strategy_guid;
    CandleBuilder m_candle_builder;

    IMarketDataGateway & m_md_gateway;
    ITradingGateway & m_tr_gateway;
    std::shared_ptr<IStrategy> m_strategy;

    EventObjectChannel<bool> m_opened_pos_channel;
    EventObjectChannel<StrategyResult> m_strategy_result;

    EventTimeseriesChannel<ProfitPriceLevels> m_price_levels_channel;
    EventTimeseriesChannel<Trade> m_trade_channel;
    EventTimeseriesChannel<double> m_price_channel;
    EventTimeseriesChannel<Candle> m_candle_channel;
    EventTimeseriesChannel<double> m_depo_channel;
    EventTimeseriesChannel<PositionResult> m_positions_channel;

    EventTimeseriesChannel<TpslPrices> m_tpsl_channel;
    EventTimeseriesChannel<StopLoss> m_trailing_stop_channel;

    StrategyChannelsRefs m_strategy_channels;

    const Symbol m_symbol;
    static constexpr double m_pos_currency_amount = 100.;
    PositionManager m_position_manager;

    const std::optional<HistoricalMDRequestData> m_historical_md_request;

    std::shared_ptr<EventObjectSubscription<WorkStatus>> m_gw_status_sub;
    bool first_price_received = false;

    std::pair<std::chrono::milliseconds, double> m_last_ts_and_price;
    std::optional<Candle> m_last_candle;
    std::optional<double> m_previous_profit;

    EventObjectChannel<WorkStatus> m_status;

    std::set<xg::Guid> m_live_md_requests;
    std::set<xg::Guid> m_pending_requests;
    std::map<xg::Guid, MarketOrder> m_pending_orders;

    bool m_stop_request_handled = false;
    WorkStatus m_status_on_stop = WorkStatus::Stopped;
    bool m_stopped = false;
    std::promise<void> m_finish_promise;

    std::optional<HistoricalMDGeneratorEvent> m_historical_md_generator;
    bool m_backtest_in_progress = false;

    EventChannel<HistoricalMDPriceEvent> m_historical_md_channel;
    EventChannel<StrategyStartRequest> m_start_ev_channel;
    EventChannel<StrategyStopRequest> m_stop_ev_channel;
    EventChannel<BarrierEvent> m_barrier_channel;

    EventTimeseriesChannel<MarketStateRenderObject> m_market_state_channel;
    MarketState m_current_market_state = MarketState::None;

    OrderManager m_orders;

    EventSubcriber m_sub;
};
