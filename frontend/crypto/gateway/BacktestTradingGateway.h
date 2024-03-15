#pragma once

#include "Events.h"
#include "ITradingGateway.h"
#include "Ohlc.h"
#include "TimeseriesPublisher.h"
#include "Volume.h"
#include <optional>

class BacktestTradingGateway : public ITradingGateway
{
public:
    static constexpr double taker_fee_rate = 0.00055;
    BacktestTradingGateway();

    void set_price_source(TimeseriesPublisher<OHLC> & publisher);

    void push_order_request(const OrderRequestEvent & order) override;
    void push_tpsl_request(const TpslRequestEvent & tpsl_ev) override;

private:
    std::optional<Trade> try_trade_tpsl(OHLC ohlc);

private:
    std::string m_symbol; // backtest GW can only trade a single symbol now
    std::optional<TpslRequestEvent> m_tpsl;
    double m_last_trade_price = 0.;
    std::shared_ptr<ISubsription> m_price_sub;

    SignedVolume m_pos_volume;
};
