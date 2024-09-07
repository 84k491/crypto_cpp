#include "BacktestTradingGateway.h"

#include "Enums.h"
#include "Events.h"
#include "Logger.h"
#include "Macros.h"
#include "Volume.h"

BacktestTradingGateway::BacktestTradingGateway()
    : m_event_consumer(std::make_shared<BacktestEventConsumer>())
{
}

void BacktestTradingGateway::set_price_source(EventTimeseriesPublisher<OHLC> & publisher)
{
    m_price_sub = publisher.subscribe(
            m_event_consumer,
            [](auto &) {},
            [this](auto, const OHLC & ohlc) {
                m_last_trade_price = ohlc.close;
                if (m_tpsl.has_value()) {
                    const auto tpsl_trade = try_trade_tpsl(ohlc);
                    if (tpsl_trade.has_value()) {
                        auto lref = m_consumers.lock();
                        for (auto & it : lref.get()) {
                            it.second.second.trade_consumer.push(TradeEvent(tpsl_trade.value()));
                        }
                        m_pos_volume = SignedVolume();
                        m_tpsl.reset();
                    }
                }
                if (m_trailing_stop.has_value()) {
                    const auto trail_stop_trade = m_trailing_stop->on_price_updated(ohlc);
                    if (trail_stop_trade.has_value()) {
                        auto lref = m_consumers.lock();
                        for (auto & it : lref.get()) {
                            it.second.second.trade_consumer.push(TradeEvent(trail_stop_trade.value()));
                        }
                        m_pos_volume = SignedVolume();
                        m_trailing_stop.reset();
                    }
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
                return tpsl.take_profit_price;
            }
            if (last_price <= tpsl.stop_loss_price) {
                return tpsl.stop_loss_price;
            }
            break;
        }
        case Side::Sell: {
            if (last_price <= tpsl.take_profit_price) {
                return tpsl.take_profit_price;
            }
            if (last_price >= tpsl.stop_loss_price) {
                return tpsl.stop_loss_price;
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
        Logger::log<LogLevel::Error>("No price sub. Did you forgot to subscribe backtest TRGW for prices?");
    }
    if (!check_consumers(req.order.symbol())) {
        UNWRAP_RET_VOID(consumer, req.response_consumer.lock());
        consumer.push(OrderResponseEvent(req.order.guid(), "No trade consumer for this symbol"));
        return;
    }

    const auto & order = req.order;
    UNWRAP_RET_VOID(consumer, req.response_consumer.lock());
    consumer.push(OrderResponseEvent(req.order.guid()));
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

    UNWRAP_RET_VOID(trade_consumer, req.trade_ev_consumer.lock());
    trade_consumer.push(TradeEvent(std::move(trade)));
}

void BacktestTradingGateway::push_tpsl_request(const TpslRequestEvent & tpsl_ev)
{
    if (!check_consumers(tpsl_ev.symbol.symbol_name)) {
        Logger::logf<LogLevel::Error>("No consumer for this symbol: {}", tpsl_ev.symbol.symbol_name);
        UNWRAP_RET_VOID(consumer, tpsl_ev.response_consumer.lock());
        consumer.push(
                TpslResponseEvent(
                        tpsl_ev.guid,
                        tpsl_ev.tpsl,
                        "No consumer for this symbol"));
        return;
    }

    m_tpsl = tpsl_ev;
    TpslResponseEvent resp_ev(m_tpsl.value().guid, tpsl_ev.tpsl);
    UNWRAP_RET_VOID(consumer, m_tpsl.value().response_consumer.lock());
    consumer.push(resp_ev);
}

void BacktestTradingGateway::push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev)
{
    UNWRAP_RET_VOID(consumer, trailing_stop_ev.response_consumer.lock());

    if (!check_consumers(trailing_stop_ev.symbol.symbol_name)) {
        Logger::logf<LogLevel::Error>("No trade consumer for this symbol: {}", trailing_stop_ev.symbol.symbol_name);
        consumer.push(
                TrailingStopLossResponseEvent(
                        trailing_stop_ev.guid,
                        trailing_stop_ev.trailing_stop_loss,
                        "No consumer for this symbol"));
        return;
    }

    m_trailing_stop = BacktestTrailingStopLoss(
        m_symbol,
        m_pos_volume,
        m_last_trade_price,
        trailing_stop_ev.trailing_stop_loss.price_distance());

    consumer.push(
            TrailingStopLossResponseEvent(
                    trailing_stop_ev.guid,
                    trailing_stop_ev.trailing_stop_loss));
    // TODO push TrailingStopUpdated?
}

void BacktestTradingGateway::register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers)
{
    Logger::logf<LogLevel::Debug>("Registering consumers for symbol: {}", symbol.symbol_name);
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

bool BacktestEventConsumer::push_to_queue(const std::any value)
{
    const auto & event = std::any_cast<LambdaEvent>(value);
    event.func();
    return true;
}

bool BacktestEventConsumer::push_to_queue_delayed(std::chrono::milliseconds, const std::any)
{
    throw std::runtime_error("Not implemented");
}

BacktestTrailingStopLoss::BacktestTrailingStopLoss(std::string symbol, SignedVolume pos_volume, double current_price, double price_delta)
    : m_symbol(std::move(symbol))
    , m_pos_volume(pos_volume)
    , m_price_delta(price_delta)
{
    const auto [vol, side] = m_pos_volume.as_unsigned_and_side();
    const int side_sign = side == Side::Buy ? 1 : -1;
    m_current_stop_price = current_price + (side_sign * price_delta);
}

std::optional<Trade> BacktestTrailingStopLoss::on_price_updated(const OHLC & ohlc)
{
    const double current_price = ohlc.close;
    const auto [vol, side] = m_pos_volume.as_unsigned_and_side();
    const int side_sign = side == Side::Buy ? 1 : -1;
    const auto possible_new_stop_price = current_price + (side_sign * m_price_delta);

    bool triggered = false;
    bool pull_up = false;
    switch (side) {
    case Side::Buy: {
        triggered = current_price < m_current_stop_price;
        pull_up = possible_new_stop_price > m_current_stop_price;
        break;
    }
    case Side::Sell: {
        triggered = current_price > m_current_stop_price;
        pull_up = possible_new_stop_price < m_current_stop_price;
        break;
    }
    case Side::Close: {
        throw std::runtime_error("Close side");
        break;
    }
    };

    if (pull_up) {
        m_current_stop_price = possible_new_stop_price;
    }
    if (!triggered) {
        return std::nullopt;
    }

    const auto opposite_side = (side == Side::Buy) ? Side::Sell : Side::Buy;

    Trade trade{
            ohlc.timestamp,
            m_symbol,
            m_current_stop_price,
            vol,
            opposite_side,
            (m_pos_volume.value() * current_price) * taker_fee_rate,
    };
    return trade;
}
