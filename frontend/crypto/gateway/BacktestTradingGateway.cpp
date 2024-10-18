#include "BacktestTradingGateway.h"

#include "Events.h"
#include "Logger.h"
#include "Macros.h"
#include "Side.h"
#include "Volume.h"

#include <variant>

BacktestTradingGateway::BacktestTradingGateway()
    : m_event_consumer(std::make_shared<BacktestEventConsumer>())
{
}

void BacktestTradingGateway::set_price_source(EventTimeseriesPublisher<OHLC> & publisher)
{
    m_price_sub = publisher.subscribe(
            m_event_consumer,
            [](auto &) {},
            [this](std::chrono::milliseconds ts, const OHLC & ohlc) {
                m_last_trade_price = ohlc.close;
                if (m_tpsl.has_value()) {
                    const auto tpsl_trade = try_trade_tpsl(ohlc);
                    if (tpsl_trade.has_value()) {
                        m_trade_publisher.push(TradeEvent(tpsl_trade.value()));
                        m_pos_volume = SignedVolume();
                        m_tpsl.reset();
                    }
                }
                if (m_trailing_stop.has_value()) {
                    const auto trade_or_sl = m_trailing_stop->on_price_updated(ohlc);
                    if (trade_or_sl.has_value()) {
                        auto lref = m_consumers.lock();
                        std::visit(
                                VariantMatcher{
                                        [&](const Trade & trade) {
                                            m_trade_publisher.push(TradeEvent(trade));
                                            for (auto & it : lref.get()) {
                                                it.second.second.trailing_stop_update_consumer.push(
                                                        TrailingStopLossUpdatedEvent(trade.symbol().symbol_name, {}, ts));
                                            }
                                            m_pos_volume = SignedVolume();
                                            m_trailing_stop.reset();
                                        },
                                        [&](const StopLoss & sl) {
                                            for (auto & it : lref.get()) {
                                                it.second.second.trailing_stop_update_consumer.push(
                                                        TrailingStopLossUpdatedEvent(sl.symbol().symbol_name, sl, ts));
                                            }
                                        }},
                                *trade_or_sl);
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
    const Side opposite_side = pos_side.opposite();
    const auto trade_price = [&]() -> std::optional<double> {
        switch (pos_side.value()) {
        case SideEnum::Buy: {
            if (last_price >= tpsl.take_profit_price) {
                return tpsl.take_profit_price;
            }
            if (last_price <= tpsl.stop_loss_price) {
                return tpsl.stop_loss_price;
            }
            break;
        }
        case SideEnum::Sell: {
            if (last_price <= tpsl.take_profit_price) {
                return tpsl.take_profit_price;
            }
            if (last_price >= tpsl.stop_loss_price) {
                return tpsl.stop_loss_price;
            }
            break;
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

    const auto & order = req.order;
    const auto ack_ev = OrderResponseEvent(req.order.symbol(), req.order.guid());
    m_order_response_publisher.push(ack_ev);
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

    m_trade_publisher.push(TradeEvent(std::move(trade)));
}

void BacktestTradingGateway::push_tpsl_request(const TpslRequestEvent & tpsl_ev)
{
    m_tpsl = tpsl_ev;
    TpslResponseEvent resp_ev(m_tpsl.value().symbol.symbol_name, m_tpsl.value().guid, tpsl_ev.tpsl);
    m_tpsl_response_publisher.push(resp_ev);
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
            m_pos_volume,
            m_last_trade_price,
            trailing_stop_ev.trailing_stop_loss);

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

BacktestTrailingStopLoss::BacktestTrailingStopLoss(SignedVolume pos_volume, double current_price, const TrailingStopLoss & trailing_stop)
    : m_pos_volume(pos_volume)
    , m_current_stop_loss(
              trailing_stop.calc_new_stop_loss(current_price, std::nullopt).value()) // there must be a value
    , m_trailing_stop(trailing_stop)
{
}

std::optional<std::variant<Trade, StopLoss>> BacktestTrailingStopLoss::on_price_updated(const OHLC & ohlc)
{
    const double tick_price = ohlc.close;
    const auto [vol, side] = m_pos_volume.as_unsigned_and_side();
    const auto new_stop_loss = m_trailing_stop.calc_new_stop_loss(tick_price, m_current_stop_loss);

    if (new_stop_loss) {
        m_current_stop_loss = new_stop_loss.value();
        return m_current_stop_loss; // SL cannot be triggered if pulled up
    }

    // TODO make a one-liner after tests
    bool triggered = false;
    switch (side.value()) {
    case SideEnum::Buy: {
        triggered = tick_price < m_current_stop_loss.stop_price();
        break;
    }
    case SideEnum::Sell: {
        triggered = tick_price > m_current_stop_loss.stop_price();
        break;
    }
    };

    if (!triggered) {
        return std::nullopt;
    }
    // Logger::logf<LogLevel::Debug>("Triggered");

    const auto opposite_side = side.opposite();
    Trade trade{
            ohlc.timestamp,
            m_trailing_stop.symbol().symbol_name,
            m_current_stop_loss.stop_price(),
            vol,
            opposite_side,
            (m_pos_volume.value() * tick_price) * taker_fee_rate,
    };
    return trade;
}
EventPublisher<OrderResponseEvent> & BacktestTradingGateway::order_response_publisher()
{
    return m_order_response_publisher;
}

EventPublisher<TradeEvent> & BacktestTradingGateway::trade_publisher()
{
    return m_trade_publisher;
}

EventPublisher<TpslResponseEvent> & BacktestTradingGateway::tpsl_response_publisher()
{
    return m_tpsl_response_publisher;
}

EventPublisher<TpslUpdatedEvent> & BacktestTradingGateway::tpsl_updated_publisher()
{
    return m_tpsl_updated_publisher;
}
