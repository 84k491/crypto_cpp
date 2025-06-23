#include "BacktestTradingGateway.h"

#include "Events.h"
#include "Logger.h"
#include "Side.h"
#include "Volume.h"

#include <variant>

BacktestTradingGateway::BacktestTradingGateway()
    : m_event_consumer(std::make_shared<BacktestEventConsumer>())
    , m_pos_volume(std::make_shared<SignedVolume>(0.))
{
}

void BacktestTradingGateway::set_price_source(EventTimeseriesChannel<double> & channel)
{
    m_price_sub = channel.subscribe(
            m_event_consumer,
            [](auto &) {},
            [this](std::chrono::milliseconds ts, const double & price) {
                on_new_price(ts, price);
            });
}

void BacktestTradingGateway::on_new_price(std::chrono::milliseconds ts, const double & price)
{
    m_last_price = price;
    m_last_ts = ts;

    if (m_tpsl.has_value()) {
        const auto tpsl_trade = try_trade_tpsl(ts, price);
        if (tpsl_trade.has_value()) {
            m_trade_channel.push(TradeEvent(tpsl_trade.value()));
            *m_pos_volume = SignedVolume();
            m_tpsl.reset();
        }
    }

    if (m_trailing_stop.has_value()) {
        const auto trade_or_sl = m_trailing_stop->on_price_updated(ts, price);
        if (trade_or_sl.has_value()) {
            std::visit(
                    VariantMatcher{
                            [&](const Trade & trade) {
                                m_trade_channel.push(TradeEvent(trade));
                                m_trailing_stop_update_channel.push(
                                        TrailingStopLossUpdatedEvent(trade.symbol_name(), {}, ts));
                                m_trailing_stop.reset();
                                *m_pos_volume = SignedVolume();
                            },
                            [&](const StopLoss & sl) {
                                m_trailing_stop_update_channel.push(
                                        TrailingStopLossUpdatedEvent(sl.symbol_name(), sl, ts));
                            }},
                    *trade_or_sl);
        }
    }

    try_trigger_conditionals(ts, price);
}

std::optional<Trade> BacktestTradingGateway::try_trade_tpsl(std::chrono::milliseconds ts, double price)
{
    if (!m_tpsl.has_value()) {
        return std::nullopt;
    }
    const auto & last_price = price;

    const auto & tpsl = m_tpsl.value().tpsl;
    const auto [pos_volume, pos_side] = m_pos_volume->as_unsigned_and_side();
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
            ts,
            m_symbol,
            trade_price.value(),
            pos_volume,
            opposite_side,
            (pos_volume.value() * last_price) * taker_fee_rate,
    };
    TpslUpdatedEvent tpsl_updated_ev(m_symbol, false);
    m_tpsl_updated_channel.push(tpsl_updated_ev);
    return trade;
}

void BacktestTradingGateway::try_trigger_conditionals(std::chrono::milliseconds ts, double price)
{
    const auto trigger = [&](const ConditionalMarketOrder & o, bool take_profit) {
        *m_pos_volume += SignedVolume{o.order().volume().value() * o.order().side().sign()};
        Trade trade{
                ts,
                m_symbol,
                o.trigger_price(),
                o.order().volume(),
                o.order().side(),
                (o.order().volume().value() * price) * taker_fee_rate,
        };
        m_trade_channel.push(std::move(trade));
        if (take_profit) {
            m_take_profit_update_channel.push({o.guid(), false});
        }
        else {
            m_stop_loss_update_channel.push({o.guid(), false});
        }
    };

    for (auto it = m_stop_losses.begin(), end = m_stop_losses.end(); it != end; ++it) {
        const auto & o = *it;

        if (price <= o.trigger_price() && o.order().side() == Side::sell()) {
            trigger(o, false);
            it = m_stop_losses.erase(it);
        }

        if (price >= o.trigger_price() && o.order().side() == Side::buy()) {
            trigger(o, false);
            it = m_stop_losses.erase(it);
        }
    }

    for (auto it = m_take_profits.begin(), end = m_take_profits.end(); it != end; ++it) {
        const auto & o = *it;

        if (price >= o.trigger_price() && o.order().side() == Side::sell()) {
            trigger(o, true);
            it = m_take_profits.erase(it);
        }

        if (price <= o.trigger_price() && o.order().side() == Side::buy()) {
            trigger(o, true);
            it = m_take_profits.erase(it);
        }
    }
}

void BacktestTradingGateway::push_order_request(const OrderRequestEvent & req)
{
    if (!m_price_sub) {
        Logger::log<LogLevel::Error>("No price sub. Did you forgot to subscribe backtest TRGW for prices?");
    }

    const auto & order = req.order;
    const auto ack_ev = OrderResponseEvent(req.order.symbol(), req.order.guid());
    m_order_response_channel.push(ack_ev);
    m_symbol = order.symbol();

    const auto & price = m_last_price;
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

    *m_pos_volume += SignedVolume(volume, order.side());

    if (m_pos_volume->value() == 0 && m_tpsl.has_value()) {
        TpslUpdatedEvent tpsl_updated_ev(m_symbol, false);
        m_tpsl_updated_channel.push(tpsl_updated_ev);
    }

    m_trade_channel.push(TradeEvent(std::move(trade)));
}

void BacktestTradingGateway::push_tpsl_request(const TpslRequestEvent & tpsl_ev)
{
    if (!m_pos_volume || m_pos_volume->value() == 0) {
        TpslResponseEvent resp_ev(
                tpsl_ev.symbol.symbol_name,
                tpsl_ev.guid,
                tpsl_ev.tpsl,
                "can not set tp/sl/ts for zero position");
        m_tpsl_response_channel.push(resp_ev);
        return;
    }

    m_tpsl = tpsl_ev;
    TpslResponseEvent resp_ev(
            m_tpsl.value().symbol.symbol_name,
            m_tpsl.value().guid,
            tpsl_ev.tpsl);
    m_tpsl_response_channel.push(resp_ev);
    TpslUpdatedEvent tpsl_updated_ev(tpsl_ev.symbol.symbol_name, true);
    m_tpsl_updated_channel.push(tpsl_updated_ev);
}

void BacktestTradingGateway::push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev)
{
    m_trailing_stop = BacktestTrailingStopLoss(
            m_pos_volume,
            m_last_price,
            trailing_stop_ev.trailing_stop_loss);

    m_trailing_stop_response_channel.push(
            TrailingStopLossResponseEvent(
                    trailing_stop_ev.guid,
                    trailing_stop_ev.trailing_stop_loss));

    m_trailing_stop_update_channel.push({m_symbol,
                                         m_trailing_stop->stop_loss(),
                                         m_last_ts});
}

void BacktestTradingGateway::push_stop_loss_request(const StopLossMarketOrder & order)
{
    m_stop_losses.push_back(order);
    m_stop_loss_update_channel.push({order.guid(), true});

    try_trigger_conditionals(order.order().signal_ts(), m_last_price);
}

void BacktestTradingGateway::push_take_profit_request(const TakeProfitMarketOrder & order)
{
    m_take_profits.push_back(order);
    m_take_profit_update_channel.push({order.guid(), true});

    try_trigger_conditionals(order.order().signal_ts(), m_last_price);
}

bool BacktestEventConsumer::push_to_queue(LambdaEvent value)
{
    value.func();
    return true;
}

bool BacktestEventConsumer::push_to_queue_delayed(std::chrono::milliseconds, LambdaEvent)
{
    throw std::runtime_error("Not implemented");
}

BacktestTrailingStopLoss::BacktestTrailingStopLoss(
        std::shared_ptr<SignedVolume> & pos_volume,
        double current_price,
        const TrailingStopLoss & trailing_stop)
    : m_pos_volume(pos_volume)
    , m_current_stop_loss(
              trailing_stop.calc_new_stop_loss(current_price, std::nullopt).value()) // there must be a value
    , m_trailing_stop(trailing_stop)
{
}

std::optional<std::variant<Trade, StopLoss>> BacktestTrailingStopLoss::on_price_updated(std::chrono::milliseconds ts, const double & price)
{
    const auto [vol, side] = m_pos_volume->as_unsigned_and_side();
    const auto new_stop_loss = m_trailing_stop.calc_new_stop_loss(price, m_current_stop_loss);

    if (new_stop_loss) {
        m_current_stop_loss = new_stop_loss.value();
        return m_current_stop_loss; // SL cannot be triggered if pulled up
    }

    // TODO make a one-liner after tests
    bool triggered = false;
    switch (side.value()) {
    case SideEnum::Buy: {
        triggered = price < m_current_stop_loss.stop_price();
        break;
    }
    case SideEnum::Sell: {
        triggered = price > m_current_stop_loss.stop_price();
        break;
    }
    };

    if (!triggered) {
        return std::nullopt;
    }

    const auto opposite_side = side.opposite();
    Trade trade{
            ts,
            m_trailing_stop.symbol_name(),
            m_current_stop_loss.stop_price(),
            vol,
            opposite_side,
            (m_pos_volume->value() * price * m_pos_volume->sign()) * taker_fee_rate,
    };
    return trade;
}

EventChannel<OrderResponseEvent> & BacktestTradingGateway::order_response_channel()
{
    return m_order_response_channel;
}

EventChannel<TradeEvent> & BacktestTradingGateway::trade_channel()
{
    return m_trade_channel;
}

EventChannel<TpslResponseEvent> & BacktestTradingGateway::tpsl_response_channel()
{
    return m_tpsl_response_channel;
}

EventChannel<TpslUpdatedEvent> & BacktestTradingGateway::tpsl_updated_channel()
{
    return m_tpsl_updated_channel;
}

EventChannel<TrailingStopLossResponseEvent> & BacktestTradingGateway::trailing_stop_response_channel()
{
    return m_trailing_stop_response_channel;
}

EventChannel<TrailingStopLossUpdatedEvent> & BacktestTradingGateway::trailing_stop_update_channel()
{
    return m_trailing_stop_update_channel;
}

EventChannel<StopLossUpdatedEvent> & BacktestTradingGateway::stop_loss_update_channel()
{
    return m_stop_loss_update_channel;
}

EventChannel<TakeProfitUpdatedEvent> & BacktestTradingGateway::take_profit_update_channel()
{
    return m_take_profit_update_channel;
}

SignedVolume BacktestTradingGateway::pos_volume() const
{
    return m_pos_volume ? *m_pos_volume : SignedVolume{};
}

void BacktestTradingGateway::cancel_stop_loss_request(xg::Guid guid)
{
    for (auto it = m_stop_losses.begin(), end = m_stop_losses.end(); it != end; ++it) {
        if (it->guid() == guid) {
            m_stop_losses.erase(it);
            m_stop_loss_update_channel.push({guid, false});
            return;
        }
    };
}

void BacktestTradingGateway::cancel_take_profit_request(xg::Guid guid)
{
    for (auto it = m_take_profits.begin(), end = m_take_profits.end(); it != end; ++it) {
        if (it->guid() == guid) {
            m_take_profits.erase(it);
            m_take_profit_update_channel.push({guid, false});
            return;
        }
    };
}
