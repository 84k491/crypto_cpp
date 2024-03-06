#pragma once

#include "Events.h"
#include "ITradingGateway.h"
#include "Ohlc.h"
#include "TimeseriesPublisher.h"

class BacktestTradingGateway : public ITradingGateway
{
public:
    static constexpr double taker_fee_rate = 0.00055;
    BacktestTradingGateway();

    void set_price_source(TimeseriesPublisher<OHLC> & publisher);

    std::optional<std::vector<Trade>> send_order_sync(const MarketOrder & order) override;
    void push_order_request(const OrderRequestEvent & order) override;

private:
    double m_last_trade_price = 0.;
    std::shared_ptr<ISubsription> m_price_sub;
};
