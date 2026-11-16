#include "BacktestingGateway.h"

#include "Events.h"
#include "Logger.h"
#include "Side.h"
#include "Volume.h"

#include <variant>

BacktestingGateway::BacktestingGateway(
        ILambdaAcceptor & event_loop,
        IMarketDataGateway & source_md_gateway,
        HistoricalMDRequestData timerange)
    : m_source_md_gateway(source_md_gateway)
    , m_timerange(timerange)
    , m_pos_volume(std::make_shared<SignedVolume>(0.))
    , m_sub(event_loop)
{
    m_sub.subscribe(
            m_source_md_gateway.historical_prices_channel(),
            [this](const HistoricalMDGeneratorEvent & ev) {
                handle_source_historical_md(ev);
            });
    m_sub.subscribe(
            m_next_tick_channel,
            [this](const NextTickRequest &) {
                send_next_tick();
            },
            Priority::Low);
    m_sub.subscribe(
            m_unsubscribe_from_live_channel,
            [this](const xg::Guid & guid) {
                stop_sending_ticks(guid);
            });
}

void BacktestingGateway::push_async_request(HistoricalMDRequest &&)
{
    LOG_ERROR("BacktestingGateway doesn't accept historical MD requests");
}

void BacktestingGateway::push_async_request(LiveMDRequest && request)
{
    if (m_live_request.has_value()) {
        LOG_ERROR("More than one MD live request");
        return;
    }

    HistoricalMDRequest historical_request(request.symbol, m_timerange);
    m_historical_request_guid = historical_request.guid;
    m_live_request = std::move(request);
    m_source_md_gateway.push_async_request(std::move(historical_request));
}

void BacktestingGateway::handle_source_historical_md(const HistoricalMDGeneratorEvent & ev)
{
    if (!m_historical_request_guid.has_value() || ev.request_guid() != m_historical_request_guid.value()) {
        return;
    }
    m_historical_request_guid.reset();

    m_historical_md_generator = ev;
    m_next_tick_channel.push({});
}

void BacktestingGateway::send_next_tick()
{
    if (!m_historical_md_generator.has_value()) {
        return;
    }

    const auto ev_opt = m_historical_md_generator->get_next();
    if (!ev_opt.has_value()) {
        m_historical_md_generator.reset();
        if (!m_any_tick_sent) {
            LOG_ERROR("Empty historical market data");
            m_status.push(WorkStatus::Panic);
            return;
        }
        m_status.push(WorkStatus::Stopped);
        return;
    }

    m_any_tick_sent = true;
    const auto & public_trade = ev_opt->public_trade;
    on_new_price(public_trade.ts(), public_trade.price());
    m_live_prices_channel.push(MDPriceEvent{public_trade});
    m_next_tick_channel.push({});
}

void BacktestingGateway::unsubscribe_from_live(xg::Guid guid)
{
    m_unsubscribe_from_live_channel.push(guid);
}

void BacktestingGateway::stop_sending_ticks(xg::Guid live_request_guid)
{
    if (!m_live_request.has_value() || m_live_request->guid != live_request_guid) {
        return;
    }
    m_live_request.reset();
    m_historical_request_guid.reset();
    m_historical_md_generator.reset();
}

EventChannel<HistoricalMDGeneratorEvent> & BacktestingGateway::historical_prices_channel()
{
    return m_historical_prices_channel;
}

EventChannel<MDPriceEvent> & BacktestingGateway::live_prices_channel()
{
    return m_live_prices_channel;
}

EventObjectChannel<WorkStatus> & BacktestingGateway::status_channel()
{
    return m_status;
}

void BacktestingGateway::on_new_price(std::chrono::milliseconds ts, const double & price)
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
                            [this, ts](const Trade & trade) {
                                m_trade_channel.push(TradeEvent(trade));
                                m_trailing_stop_update_channel.push(
                                        TrailingStopLossUpdatedEvent(
                                                trade.symbol_name(),
                                                {},
                                                ts));
                                m_trailing_stop.reset();
                                *m_pos_volume = SignedVolume();
                            },
                            [this, ts](const StopLoss & sl) {
                                m_trailing_stop_update_channel.push(
                                        TrailingStopLossUpdatedEvent(
                                                sl.symbol_name(),
                                                sl,
                                                ts));
                            }},
                    *trade_or_sl);
        }
    }

    try_trigger_conditionals(ts, price);
}

std::optional<Trade> BacktestingGateway::try_trade_tpsl(std::chrono::milliseconds ts, double price)
{
    if (!m_tpsl.has_value()) {
        return std::nullopt;
    }
    const auto & last_price = price;

    double take_profit_price = m_tpsl.value().take_profit_price;
    double stop_loss_price = m_tpsl.value().stop_loss_price;
    const auto [pos_volume, pos_side] = m_pos_volume->as_unsigned_and_side();
    const Side opposite_side = pos_side.opposite();
    const auto trade_price = [&]() -> std::optional<double> {
        switch (pos_side.value()) {
        case SideEnum::Buy: {
            if (last_price >= take_profit_price) {
                return take_profit_price;
            }
            if (last_price <= stop_loss_price) {
                return stop_loss_price;
            }
            break;
        }
        case SideEnum::Sell: {
            if (last_price <= take_profit_price) {
                return take_profit_price;
            }
            if (last_price >= stop_loss_price) {
                return stop_loss_price;
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
            m_tpsl->guid,
            trade_price.value(),
            pos_volume,
            opposite_side,
            (pos_volume.value() * last_price) * taker_fee_rate,
    };
    TpslUpdatedEvent tpsl_updated_ev{
            m_symbol,
            m_tpsl->guid,
            false,
            true};
    m_tpsl_updated_channel.push(tpsl_updated_ev);
    return trade;
}

void BacktestingGateway::try_trigger_conditionals(std::chrono::milliseconds ts, double price)
{
    const auto trigger = [&](const ConditionalMarketOrder & o, bool take_profit) {
        *m_pos_volume += SignedVolume{o.target_volume().value() * o.side().sign()};
        Trade trade{
                ts,
                m_symbol,
                o.guid(),
                o.trigger_price(),
                o.target_volume(),
                o.side(),
                (o.target_volume().value() * price) * taker_fee_rate,
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

        if (o.side() == Side::sell() && price <= o.trigger_price()) {
            trigger(o, false);
            it = m_stop_losses.erase(it);
            continue;
        }

        if (o.side() == Side::buy() && price >= o.trigger_price()) {
            trigger(o, false);
            it = m_stop_losses.erase(it);
            continue;
        }
    }

    for (auto it = m_take_profits.begin(), end = m_take_profits.end(); it != end; ++it) {
        const auto & o = *it;

        if (o.side() == Side::sell() && price >= o.trigger_price()) {
            trigger(o, true);
            it = m_take_profits.erase(it);
            continue;
        }

        if (o.side() == Side::buy() && price <= o.trigger_price()) {
            trigger(o, true);
            it = m_take_profits.erase(it);
            continue;
        }
    }
}

void BacktestingGateway::push_order_request(const OrderRequestEvent & req)
{
    const auto & order = req.order;
    const auto ack_ev = OrderResponseEvent(req.order.symbol(), req.order.guid());
    m_order_response_channel.push(ack_ev);
    m_symbol = order.symbol();

    const auto & price = m_last_price;
    const auto volume = order.target_volume();

    const double currency_spent = price * volume.value();
    const double fee = currency_spent * taker_fee_rate;
    Trade trade{
            std::chrono::duration_cast<std::chrono::milliseconds>(order.signal_ts()),
            order.symbol(),
            order.guid(),
            price,
            volume,
            order.side(),
            fee,
    };

    *m_pos_volume += SignedVolume(volume, order.side());

    if (m_pos_volume->value() == 0 && m_tpsl.has_value()) {
        TpslUpdatedEvent tpsl_updated_ev(
                m_symbol,
                m_tpsl->guid,
                false,
                false);
        m_tpsl_updated_channel.push(tpsl_updated_ev);
    }

    m_trade_channel.push(TradeEvent(std::move(trade)));
}

void BacktestingGateway::push_tpsl_request(const TpslRequestEvent & tpsl_ev)
{
    if (!m_pos_volume || m_pos_volume->value() == 0) {
        TpslUpdatedEvent tpsl_updated_ev(
                tpsl_ev.symbol.symbol_name,
                tpsl_ev.guid,
                true,
                false,
                "can not set tp/sl/ts for zero position");
        m_tpsl_updated_channel.push(tpsl_updated_ev);
        return;
    }

    m_tpsl = tpsl_ev;
    TpslUpdatedEvent tpsl_updated_ev(
            tpsl_ev.symbol.symbol_name,
            tpsl_ev.guid,
            true,
            false);
    m_tpsl_updated_channel.push(tpsl_updated_ev);
}

void BacktestingGateway::push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev)
{
    m_trailing_stop = BacktestTrailingStopLoss(
            m_pos_volume,
            m_last_price,
            trailing_stop_ev.trailing_stop_loss);

    m_trailing_stop_update_channel.push({m_symbol,
                                         m_trailing_stop->stop_loss(),
                                         m_last_ts});
}

void BacktestingGateway::push_stop_loss_request(const StopLossMarketOrder & order)
{
    m_stop_losses.push_back(order);
    m_stop_loss_update_channel.push({order.guid(), true});

    try_trigger_conditionals(order.signal_ts(), m_last_price);
}

void BacktestingGateway::push_take_profit_request(const TakeProfitMarketOrder & order)
{
    m_take_profits.push_back(order);
    m_take_profit_update_channel.push({order.guid(), true});

    try_trigger_conditionals(order.signal_ts(), m_last_price);
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
    const auto new_stop_loss = m_trailing_stop.calc_new_stop_loss(
            price,
            m_current_stop_loss);

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
            m_trailing_stop.guid(),
            m_current_stop_loss.stop_price(),
            vol,
            opposite_side,
            (m_pos_volume->value() * price * m_pos_volume->sign()) * taker_fee_rate,
    };
    return trade;
}

EventChannel<OrderResponseEvent> & BacktestingGateway::order_response_channel()
{
    return m_order_response_channel;
}

EventChannel<TradeEvent> & BacktestingGateway::trade_channel()
{
    return m_trade_channel;
}

EventChannel<TpslUpdatedEvent> & BacktestingGateway::tpsl_updated_channel()
{
    return m_tpsl_updated_channel;
}

EventChannel<TrailingStopLossUpdatedEvent> & BacktestingGateway::trailing_stop_update_channel()
{
    return m_trailing_stop_update_channel;
}

EventChannel<StopLossUpdatedEvent> & BacktestingGateway::stop_loss_update_channel()
{
    return m_stop_loss_update_channel;
}

EventChannel<TakeProfitUpdatedEvent> & BacktestingGateway::take_profit_update_channel()
{
    return m_take_profit_update_channel;
}

SignedVolume BacktestingGateway::pos_volume() const
{
    return m_pos_volume ? *m_pos_volume : SignedVolume{};
}

void BacktestingGateway::cancel_stop_loss_request(xg::Guid guid)
{
    for (auto it = m_stop_losses.begin(), end = m_stop_losses.end(); it != end; ++it) {
        if (it->guid() == guid) {
            m_stop_losses.erase(it);
            m_stop_loss_update_channel.push({guid, false});
            return;
        }
    };
}

void BacktestingGateway::cancel_take_profit_request(xg::Guid guid)
{
    for (auto it = m_take_profits.begin(), end = m_take_profits.end(); it != end; ++it) {
        if (it->guid() == guid) {
            m_take_profits.erase(it);
            m_take_profit_update_channel.push({guid, false});
            return;
        }
    };
}
