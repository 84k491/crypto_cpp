#pragma once

#include "EventTimeseriesChannel.h"
#include "Events.h"
#include "ITradingGateway.h"
#include "Ohlc.h"
#include "Volume.h"

#include <optional>

class BacktestEventConsumer : public IEventConsumer<LambdaEvent>
{
    // IEventConsumer<LambdaEvent>
    bool push_to_queue(const std::any value) override;
    bool push_to_queue_delayed(std::chrono::milliseconds delay, const std::any value) override;
};

class BacktestTrailingStopLoss
{
    static constexpr double taker_fee_rate = 0.001; // TODO take it from BTGW
public:
    BacktestTrailingStopLoss(std::shared_ptr<SignedVolume> & pos_volume, double current_price, const TrailingStopLoss & trailing_stop);

    [[nodiscard("PossibleTrade or updated StopLoss")]]
    std::optional<std::variant<Trade, StopLoss>> on_price_updated(std::chrono::milliseconds ts, const double & price);

    auto stop_loss() const { return m_current_stop_loss; }

private:
    std::shared_ptr<SignedVolume> m_pos_volume;
    StopLoss m_current_stop_loss;
    TrailingStopLoss m_trailing_stop;
};

class BacktestTradingGateway : public ITradingGateway
{
public:
    static constexpr double taker_fee_rate = 0.001;
    BacktestTradingGateway();

    void set_price_source(EventTimeseriesChannel<double> & channel);

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;
    void push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev) override;

    EventChannel<OrderResponseEvent> & order_response_channel() override;
    EventChannel<TradeEvent> & trade_channel() override;
    EventChannel<TpslResponseEvent> & tpsl_response_channel() override;
    EventChannel<TpslUpdatedEvent> & tpsl_updated_channel() override;
    EventChannel<TrailingStopLossResponseEvent> & trailing_stop_response_channel() override;
    EventChannel<TrailingStopLossUpdatedEvent> & trailing_stop_update_channel() override;

private:
    std::optional<Trade> try_trade_tpsl(std::chrono::milliseconds ts, double price);

private:
    std::shared_ptr<BacktestEventConsumer> m_event_consumer;

    std::string m_symbol; // backtest GW can only trade a single symbol now
    std::optional<TpslRequestEvent> m_tpsl;
    double m_last_price = 0.;
    std::chrono::milliseconds m_last_ts = {};
    std::shared_ptr<ISubsription> m_price_sub;
    std::optional<BacktestTrailingStopLoss> m_trailing_stop;

    std::shared_ptr<SignedVolume> m_pos_volume;

    EventChannel<OrderResponseEvent> m_order_response_channel;
    EventChannel<TradeEvent> m_trade_channel;
    EventChannel<TpslResponseEvent> m_tpsl_response_channel;
    EventChannel<TpslUpdatedEvent> m_tpsl_updated_channel;
    EventChannel<TrailingStopLossResponseEvent> m_trailing_stop_response_channel;
    EventChannel<TrailingStopLossUpdatedEvent> m_trailing_stop_update_channel;
};
