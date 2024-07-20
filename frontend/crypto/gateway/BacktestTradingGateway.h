#pragma once

#include "Events.h"
#include "Guarded.h"
#include "ITradingGateway.h"
#include "Ohlc.h"
#include "TimeseriesPublisher.h"
#include "Volume.h"

#include <optional>

class BacktestTradingGateway
    : public ITradingGateway
    , public IEventConsumer<LambdaEvent>
{
public:
    static constexpr double taker_fee_rate = 0.00055;
    BacktestTradingGateway();

    void set_price_source(TimeseriesPublisher<OHLC> & publisher);

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;

    void register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers) override;
    void unregister_consumers(xg::Guid guid) override;

private:
    bool check_consumers(const std::string & symbol);
    std::optional<Trade> try_trade_tpsl(OHLC ohlc);

private:
    // IEventConsumer<LambdaEvent>
    bool push_to_queue(const std::any value) override;
    bool push_to_queue_delayed(std::chrono::milliseconds delay, const std::any value) override;

private:
    std::string m_symbol; // backtest GW can only trade a single symbol now
    std::optional<TpslRequestEvent> m_tpsl;
    double m_last_trade_price = 0.;
    std::shared_ptr<ISubsription> m_price_sub;

    SignedVolume m_pos_volume;

    Guarded<std::map<std::string, std::pair<xg::Guid, TradingGatewayConsumers>>> m_consumers;
};
