#pragma once

#include "ConditionalOrders.h"
#include "EventChannel.h"
#include "EventLoopSubscriber.h"
#include "EventObjectChannel.h"
#include "Events.h"
#include "IMarketDataGateway.h"
#include "ITradingGateway.h"
#include "Volume.h"
#include "WorkStatus.h"

#include <chrono>
#include <optional>
#include <variant>

class BacktestTrailingStopLoss
{
    static constexpr double taker_fee_rate = 0.001; // TODO take it from BTGW
public:
    BacktestTrailingStopLoss(std::shared_ptr<SignedVolume> & pos_volume, double current_price, const TrailingStopLoss & trailing_stop);

    [[nodiscard("PossibleTrade or updated StopLoss")]]
    std::optional<std::variant<Trade, StopLoss>> on_price_updated(std::chrono::milliseconds ts, const double & price);

    auto stop_loss() const { return m_current_stop_loss; }
    auto guid() const { return m_trailing_stop.guid(); }

private:
    std::shared_ptr<SignedVolume> m_pos_volume;
    StopLoss m_current_stop_loss;
    TrailingStopLoss m_trailing_stop;
};

class BacktestingGateway
    : public IMarketDataGateway
    , public ITradingGateway
{
    struct NextTickRequest
    {
    };

    struct PlaybackWakeUp
    {
        // A delayed event can't be cancelled, so a wake-up planned before a playback restart is still delivered.
        // It carries the generation it was planned with and is dropped if the gateway has a newer one.
        size_t playback_generation;
    };

public:
    static constexpr double taker_fee_rate = 0.001;
    static constexpr std::chrono::milliseconds playback_wake_up_interval{100};

    BacktestingGateway(
            ILambdaAcceptor & event_loop,
            IMarketDataGateway & source_md_gateway,
            HistoricalMDRequestData timerange);

    void push_async_request(HistoricalMDRequest && request) override;
    void push_async_request(LiveMDRequest && request) override;

    EventChannel<HistoricalMDGeneratorEvent> & historical_prices_channel() override;
    EventChannel<MDPriceEvent> & live_prices_channel() override;

    void unsubscribe_from_live(xg::Guid guid) override;

    EventObjectChannel<WorkStatus> & status_channel() override;

    void push_playback_settings(const PlaybackSettingsEvent & settings);
    void push_playback_step(const PlaybackStepEvent & step);

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;
    void push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev) override;
    void push_take_profit_request(const TakeProfitMarketOrder & order) override;
    void push_stop_loss_request(const StopLossMarketOrder & order) override;
    void cancel_stop_loss_request(xg::Guid guid) override;
    void cancel_take_profit_request(xg::Guid guid) override;

    EventChannel<OrderResponseEvent> & order_response_channel() override;
    EventChannel<TradeEvent> & trade_channel() override;
    EventChannel<TpslUpdatedEvent> & tpsl_updated_channel() override;
    EventChannel<TrailingStopLossUpdatedEvent> & trailing_stop_update_channel() override;
    EventChannel<StopLossUpdatedEvent> & stop_loss_update_channel() override;
    EventChannel<TakeProfitUpdatedEvent> & take_profit_update_channel() override;

    SignedVolume pos_volume() const;

    void on_new_price(std::chrono::milliseconds ts, const double & price);

private:
    void handle_source_historical_md(const HistoricalMDGeneratorEvent & ev);
    void request_next_tick();
    void send_next_tick();
    void stop_sending_ticks(xg::Guid live_request_guid);

    void apply_playback_settings(const PlaybackSettingsEvent & settings);
    void restart_playback();
    void step_playback();
    void handle_playback_wake_up(const PlaybackWakeUp & wake_up);
    std::chrono::milliseconds playback_position() const;

    std::optional<Trade> try_trade_tpsl(std::chrono::milliseconds ts, double price);
    void try_trigger_conditionals(std::chrono::milliseconds ts, double price);

private:
    IMarketDataGateway & m_source_md_gateway;
    const HistoricalMDRequestData m_timerange;

    std::optional<LiveMDRequest> m_live_request;
    std::optional<xg::Guid> m_historical_request_guid;
    std::optional<HistoricalMDGeneratorEvent> m_historical_md_generator;
    bool m_next_tick_requested = false;

    PlaybackMode m_playback_mode = PlaybackMode::Rapid;
    std::chrono::milliseconds m_playback_delta = std::chrono::minutes{1};
    std::optional<std::chrono::milliseconds> m_playback_limit;
    std::chrono::milliseconds m_playback_clock_start_position = {};
    std::chrono::steady_clock::time_point m_playback_clock_start;
    size_t m_playback_generation = 0;

    EventChannel<HistoricalMDGeneratorEvent> m_historical_prices_channel;
    EventChannel<MDPriceEvent> m_live_prices_channel;
    EventObjectChannel<WorkStatus> m_status;
    EventChannel<NextTickRequest> m_next_tick_channel;
    EventChannel<xg::Guid> m_unsubscribe_from_live_channel;

    EventChannel<PlaybackSettingsEvent> m_playback_settings_channel;
    EventChannel<PlaybackStepEvent> m_playback_step_channel;
    EventChannel<PlaybackWakeUp> m_playback_wake_up_channel;

    std::string m_symbol; // backtest GW can only trade a single symbol now
    std::optional<TpslRequestEvent> m_tpsl;
    double m_last_price = 0.;
    std::chrono::milliseconds m_last_ts = {};
    std::optional<BacktestTrailingStopLoss> m_trailing_stop;

    std::shared_ptr<SignedVolume> m_pos_volume;

    EventChannel<OrderResponseEvent> m_order_response_channel;
    EventChannel<TradeEvent> m_trade_channel;
    EventChannel<TpslUpdatedEvent> m_tpsl_updated_channel;
    EventChannel<TrailingStopLossUpdatedEvent> m_trailing_stop_update_channel;
    EventChannel<StopLossUpdatedEvent> m_stop_loss_update_channel;
    EventChannel<TakeProfitUpdatedEvent> m_take_profit_update_channel;

    std::list<StopLossMarketOrder> m_stop_losses;
    std::list<TakeProfitMarketOrder> m_take_profits;

    EventSubcriber m_sub;
};
