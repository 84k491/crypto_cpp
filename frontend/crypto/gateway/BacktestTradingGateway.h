#pragma once

#include "EventTimeseriesPublisher.h"
#include "Events.h"
#include "Guarded.h"
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
    static constexpr double taker_fee_rate = 0.00055; // TODO take it from BTGW
public:
    BacktestTrailingStopLoss(std::shared_ptr<SignedVolume> & pos_volume, double current_price, const TrailingStopLoss & trailing_stop);

    [[nodiscard("PossibleTrade or updated StopLoss")]]
    std::optional<std::variant<Trade, StopLoss>> on_price_updated(const OHLC & ohlc);

private:
    std::shared_ptr<SignedVolume> m_pos_volume;
    StopLoss m_current_stop_loss;
    TrailingStopLoss m_trailing_stop;
};

class BacktestTradingGateway : public ITradingGateway
{
public:
    static constexpr double taker_fee_rate = 0.00055;
    BacktestTradingGateway();

    void set_price_source(EventTimeseriesPublisher<OHLC> & publisher);

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;
    void push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev) override;

    EventPublisher<OrderResponseEvent> & order_response_publisher() override;
    EventPublisher<TradeEvent> & trade_publisher() override;
    EventPublisher<TpslResponseEvent> & tpsl_response_publisher() override;
    EventPublisher<TpslUpdatedEvent> & tpsl_updated_publisher() override;
    EventPublisher<TrailingStopLossResponseEvent> & trailing_stop_response_publisher() override;
    EventPublisher<TrailingStopLossUpdatedEvent> & trailing_stop_update_publisher() override;

private:
    std::optional<Trade> try_trade_tpsl(OHLC ohlc);

private:
    std::shared_ptr<BacktestEventConsumer> m_event_consumer;

    std::string m_symbol; // backtest GW can only trade a single symbol now
    std::optional<TpslRequestEvent> m_tpsl;
    double m_last_price = 0.;
    std::shared_ptr<ISubsription> m_price_sub;
    std::optional<BacktestTrailingStopLoss> m_trailing_stop;

    std::shared_ptr<SignedVolume> m_pos_volume;

    EventPublisher<OrderResponseEvent> m_order_response_publisher;
    EventPublisher<TradeEvent> m_trade_publisher;
    EventPublisher<TpslResponseEvent> m_tpsl_response_publisher;
    EventPublisher<TpslUpdatedEvent> m_tpsl_updated_publisher;
    EventPublisher<TrailingStopLossResponseEvent> m_trailing_stop_response_publisher;
    EventPublisher<TrailingStopLossUpdatedEvent> m_trailing_stop_update_publisher;
};
