#include "BacktestTradingGateway.h"

#include "Enums.h"
#include "Events.h"
#include "Volume.h"

BacktestTradingGateway::BacktestTradingGateway() = default;

void BacktestTradingGateway::set_price_source(TimeseriesPublisher<OHLC> & publisher)
{
    m_price_sub = publisher.subscribe(
            [](auto &) {},
            [this](auto, const OHLC & ohlc) {
                m_last_trade_price = ohlc.close;
                const auto tpsl_trade = try_trade_tpsl(ohlc);
                if (m_tpsl.has_value() && tpsl_trade.has_value()) {
                    auto lref = m_consumers.lock();
                    for (auto & it : lref.get()) {
                        it.second.second.trade_consumer.push_in_this_thread(TradeEvent(tpsl_trade.value()));
                    }
                    m_pos_volume = SignedVolume();
                    m_tpsl.reset();
                }
            });
}

std::optional<Trade> BacktestTradingGateway::try_trade_tpsl(OHLC ohlc)
{
    if (!m_tpsl.has_value()) {
        return std::nullopt;
    }
    const auto & last_price = ohlc.close;

    const auto & tpsl = m_tpsl.value().tpsl;
    const auto [pos_volume, pos_side] = m_pos_volume.as_unsigned_and_side();
    const Side opposite_side = pos_side == Side::Sell ? Side::Buy : Side::Sell;
    const auto trade_price = [&]() -> std::optional<double> {
        switch (pos_side) {
        case Side::Buy: {
            if (last_price >= tpsl.take_profit_price) {
                return last_price;
            }
            if (last_price <= tpsl.stop_loss_price) {
                return last_price;
            }
            break;
        }
        case Side::Sell: {
            if (last_price <= tpsl.take_profit_price) {
                return last_price;
            }
            if (last_price >= tpsl.stop_loss_price) {
                return last_price;
            }
            break;
        }
        case Side::Close: {
        }
        }
        return std::nullopt;
    }();

    if (!trade_price.has_value()) {
        return std::nullopt;
    }

    Trade trade{
            ohlc.timestamp,
            m_symbol,
            trade_price.value(),
            pos_volume,
            opposite_side,
            (pos_volume.value() * last_price) * taker_fee_rate,
    };
    return trade;
}

void BacktestTradingGateway::push_order_request(const OrderRequestEvent & req)
{
    if (!m_price_sub) {
        std::cout << "No price sub. Did you forgot to subscribe backtest TRGW for prices?" << std::endl;
    }
    if (!check_consumers(req.order.symbol())) {
        req.event_consumer->push(OrderResponseEvent(req.order.guid(), "No trade consumer for this symbol"));
        return;
    }

    const auto & order = req.order;
    req.event_consumer->push_in_this_thread(OrderResponseEvent(req.order.guid()));
    m_symbol = order.symbol();

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

    m_pos_volume += SignedVolume(volume, order.side());

    req.trade_ev_consumer->push_in_this_thread(TradeEvent(std::move(trade)));
}

void BacktestTradingGateway::push_tpsl_request(const TpslRequestEvent & tpsl_ev)
{
    if (!check_consumers(tpsl_ev.symbol.symbol_name)) {
        std::cout << "ERROR: No consumer for this symbol" << std::endl;
        tpsl_ev.event_consumer->push(TpslResponseEvent(tpsl_ev.guid, tpsl_ev.tpsl, "No consumer for this symbol"));
        return;
    }

    m_tpsl = tpsl_ev;
    TpslResponseEvent resp_ev(m_tpsl.value().guid, tpsl_ev.tpsl);
    m_tpsl.value().event_consumer->push_in_this_thread(resp_ev);
}

void BacktestTradingGateway::register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers)
{
    auto lref = m_consumers.lock();
    lref.get().emplace(symbol.symbol_name, std::make_pair(guid, consumers));
}

void BacktestTradingGateway::unregister_consumers(xg::Guid guid)
{
    auto lref = m_consumers.lock();
    for (auto it = lref.get().begin(), end = lref.get().end(); it != end; ++it) {
        if (it->second.first == guid) {
            lref.get().erase(it);
            return;
        }
    }
}

bool BacktestTradingGateway::check_consumers(const std::string & symbol)
{
    auto lref = m_consumers.lock();
    auto it = lref.get().find(symbol);
    return it != lref.get().end();
}
