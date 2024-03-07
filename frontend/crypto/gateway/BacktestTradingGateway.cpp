#include "BacktestTradingGateway.h"

#include "Events.h"
#include "Volume.h"

BacktestTradingGateway::BacktestTradingGateway() = default;

void BacktestTradingGateway::set_price_source(TimeseriesPublisher<OHLC> & publisher)
{
    m_price_sub = publisher.subscribe(
            [](auto &) {},
            [this](auto, const OHLC & ohlc) {
                m_last_trade_price = ohlc.close;
            });
}

void BacktestTradingGateway::push_order_request(const OrderRequestEvent & req)
{
    if (!m_price_sub) {
        std::cout << "No price sub. Did you forgot to subscribe backtest TRGW for prices?" << std::endl;
    }

    const auto & order = req.order;
    req.event_consumer->push_in_this_thread(OrderAcceptedEvent(order));

    const auto & price = m_last_trade_price;
    const auto volume = order.volume();

    const double currency_spent = price * volume.value();
    const double fee = currency_spent * taker_fee_rate;
    Trade trade{
            std::chrono::duration_cast<std::chrono::milliseconds>(order.signal_ts()),
            order.symbol(),
            price,
            volume,
            order.side(),
            fee,
    };

    req.trade_ev_consumer->push_in_this_thread(TradeEvent(std::move(trade)));
}
