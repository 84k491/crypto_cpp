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
    BacktestTrailingStopLoss(std::string symbol, SignedVolume pos_volume, double current_price, double price_delta);

    [[nodiscard("PossibleTrade")]] std::optional<Trade> on_price_updated(const OHLC & ohlc);

private:
    std::string m_symbol;
    SignedVolume m_pos_volume; // TODO make it ref
    double m_current_stop_price = 0.;
    double m_price_delta = 0.;
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

    void register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers) override;
    void unregister_consumers(xg::Guid guid) override;

private:
    bool check_consumers(const std::string & symbol);
    std::optional<Trade> try_trade_tpsl(OHLC ohlc);

private:
    std::shared_ptr<BacktestEventConsumer> m_event_consumer;

    std::string m_symbol; // backtest GW can only trade a single symbol now
    std::optional<TpslRequestEvent> m_tpsl;
    double m_last_trade_price = 0.;
    std::shared_ptr<ISubsription> m_price_sub;
    std::optional<BacktestTrailingStopLoss> m_trailing_stop;

    SignedVolume m_pos_volume;

    Guarded<std::map<std::string, std::pair<xg::Guid, TradingGatewayConsumers>>> m_consumers;
};
