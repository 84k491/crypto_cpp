#include "OrderManager.h"

#include "Events.h"
#include "Logger.h"

#include "fmt/format.h"

#include <cassert>
#include <memory>
#include <utility>

OrderManager::OrderManager(
        Symbol symbol,
        std::shared_ptr<EventLoop> & event_loop,
        ITradingGateway & tr_gateway)
    : m_symbol(std::move(symbol))
    , m_tr_gateway(tr_gateway)
    , m_event_loop(event_loop)
    , m_sub(event_loop)
{
    m_sub.subscribe(
            m_tr_gateway.order_response_channel(),
            [this](const OrderResponseEvent & e) {
                on_order_response(e);
            });

    // there are no fees in order response
    m_sub.subscribe(
            m_tr_gateway.trade_channel(),
            [this](const TradeEvent & e) {
                on_trade(e);
            });

    m_sub.subscribe(
            m_tr_gateway.take_profit_update_channel(),
            [this](const TakeProfitUpdatedEvent & ev) {
                on_take_profit_response(ev);
            });

    m_sub.subscribe(
            m_tr_gateway.stop_loss_update_channel(),
            [this](const StopLossUpdatedEvent & ev) {
                on_stop_loss_reposnse(ev);
            });

    m_sub.subscribe(
            m_tr_gateway.trailing_stop_update_channel(),
            [this](const TrailingStopLossUpdatedEvent & ev) {
                on_trailing_stop_response(ev);
            });

    m_sub.subscribe(
            m_tr_gateway.tpsl_updated_channel(),
            [this](const TpslUpdatedEvent & ev) {
                on_tpsl_reposnse(ev);
            });
}

std::variant<SignedVolume, std::string> OrderManager::adjusted_volume(SignedVolume vol)
{
    const std::optional<SignedVolume> adjusted_target_volume_opt = m_symbol.get_qty_floored(vol);
    if (!adjusted_target_volume_opt.has_value()) {
        const std::string er = fmt::format(
                "ERROR can't get proper volume on open_or_move, target_absolute_volume = {}, qty_step = {}",
                vol.value(),
                m_symbol.lot_size_filter.qty_step);
        return {er};
    }
    const SignedVolume adjusted_target_volume = adjusted_target_volume_opt.value();

    if (0. == adjusted_target_volume.value()) {
        const std::string er = "Zero volume on market order send";
        return {er};
    }

    return {adjusted_target_volume};
}

EventObjectChannel<std::shared_ptr<MarketOrder>> & OrderManager::send_market_order(double price, SignedVolume vol, std::chrono::milliseconds ts)
{
    const auto adj_vol_var = adjusted_volume(vol);
    if (std::holds_alternative<std::string>(adj_vol_var)) {
        m_error_channel.push(std::get<std::string>(adj_vol_var));
        // TODO what to return here?
    }
    const auto adj_vol = std::get<SignedVolume>(adj_vol_var);

    const auto order = std::make_shared<MarketOrder>(
            m_symbol.symbol_name,
            price,
            adj_vol,
            ts);

    OrderRequestEvent or_event{*order};

    const auto [it, success] = m_orders.emplace(
            order->guid(),
            EventObjectChannel<std::shared_ptr<MarketOrder>>{});
    assert(success); // TODO handle error. this won't work in release

    EventObjectChannel<std::shared_ptr<MarketOrder>> & order_channel = it->second.ch;
    order_channel.update([&](auto & order_ptr) {
        order_ptr = order;
    });
    order_channel.set_on_sub_count_changed([this, guid = order->guid()](size_t sub_cnt) {
        if (sub_cnt == 0) {
            m_orders.erase(guid);
        }
    });

    m_tr_gateway.push_order_request(or_event);

    return order_channel;
}

EventObjectChannel<std::shared_ptr<TakeProfitMarketOrder>> & OrderManager::send_take_profit(
        double price,
        SignedVolume vol,
        std::chrono::milliseconds ts)
{
    const auto adj_vol_var = adjusted_volume(vol);
    if (std::holds_alternative<std::string>(adj_vol_var)) {
        m_error_channel.push(std::get<std::string>(adj_vol_var));
    }
    const auto adj_vol = std::get<SignedVolume>(adj_vol_var);

    const auto [v, s] = adj_vol.as_unsigned_and_side();
    const auto tpmo = std::make_shared<TakeProfitMarketOrder>(
            m_symbol.symbol_name,
            price,
            v,
            s,
            ts);

    const auto [it, success] = m_take_profits.emplace(
            tpmo->guid(),
            EventObjectChannel<std::shared_ptr<TakeProfitMarketOrder>>{});
    auto & ch = it->second;
    ch.update([&](auto & order_ptr) {
        order_ptr = tpmo;
    });
    ch.set_on_sub_count_changed([this, guid = tpmo->guid()](size_t sub_cnt) {
        if (sub_cnt == 0) {
            cancel_take_profit(guid);
            m_take_profits.erase(guid);
        }
    });

    m_tr_gateway.push_take_profit_request(*tpmo);

    return ch;
}

EventObjectChannel<std::shared_ptr<StopLossMarketOrder>> & OrderManager::send_stop_loss(
        double price,
        SignedVolume vol,
        std::chrono::milliseconds ts)
{
    const auto adj_vol_var = adjusted_volume(vol);
    if (std::holds_alternative<std::string>(adj_vol_var)) {
        m_error_channel.push(std::get<std::string>(adj_vol_var));
    }
    const auto adj_vol = std::get<SignedVolume>(adj_vol_var);

    const auto [v, s] = adj_vol.as_unsigned_and_side();
    const auto slmo = std::make_shared<StopLossMarketOrder>(
            m_symbol.symbol_name,
            price,
            v,
            s,
            ts);

    const auto [it, success] = m_stop_losses.emplace(
            slmo->guid(),
            EventObjectChannel<std::shared_ptr<StopLossMarketOrder>>{});
    auto & ch = it->second;
    ch.update([&](auto & order_ptr) {
        order_ptr = slmo;
    });
    ch.set_on_sub_count_changed([this, guid = slmo->guid()](size_t sub_cnt) {
        if (sub_cnt == 0) {
            cancel_stop_loss(guid);
            m_stop_losses.erase(guid);
        }
    });

    m_tr_gateway.push_stop_loss_request(*slmo);

    return ch;
}

void OrderManager::cancel_take_profit(xg::Guid guid)
{
    const auto it = m_take_profits.find(guid);
    if (it == m_take_profits.end()) {
        Logger::logf<LogLevel::Error>("No TP to cancel: {}", guid);
        return;
    }
    it->second.update([&](std::shared_ptr<TakeProfitMarketOrder> & tp) {
        tp->cancel();
    });
    m_tr_gateway.cancel_take_profit_request(guid);
}

void OrderManager::cancel_stop_loss(xg::Guid guid)
{
    const auto it = m_stop_losses.find(guid);
    if (it == m_stop_losses.end()) {
        Logger::logf<LogLevel::Error>("No SL to cancel: {}", guid);
        return;
    }
    it->second.update([&](std::shared_ptr<StopLossMarketOrder> & sl) {
        sl->cancel();
    });
    m_tr_gateway.cancel_stop_loss_request(guid);
}

void OrderManager::on_take_profit_response(const TakeProfitUpdatedEvent & r)
{
    const auto it = m_take_profits.find(r.guid);
    if (it == m_take_profits.end()) {
        if (r.active) {
            Logger::logf<LogLevel::Error>("Can't find active tp on response: {}", r.guid);
            m_error_channel.push(fmt::format("Can't find active tp on response: {}", r.guid.str()));
        }
        return;
    }
    auto & ch = it->second;
    ch.update([&](std::shared_ptr<TakeProfitMarketOrder> & tp_ptr) {
        tp_ptr->on_state_changed(r.active);
    });
}

void OrderManager::on_stop_loss_reposnse(const StopLossUpdatedEvent & r)
{
    const auto it = m_stop_losses.find(r.guid);
    if (it == m_stop_losses.end()) {
        if (r.active) {
            Logger::logf<LogLevel::Error>("Can't find actiev sl on response: {}", r.guid);
            m_error_channel.push(fmt::format("Can't find actiev sl on response: {}", r.guid.str()));
        }
        return;
    }
    auto & ch = it->second;
    ch.update([&](std::shared_ptr<StopLossMarketOrder> & sl_ptr) {
        sl_ptr->on_state_changed(r.active);
    });
}

void OrderManager::on_order_response(const OrderResponseEvent & response)
{
    const auto it = m_orders.find(response.request_guid);
    if (it == m_orders.end()) {
        Logger::logf<LogLevel::Warning>("unsolicited OrderResponseEvent {}", response.request_guid);
        m_error_channel.push(fmt::format("unsolicited OrderResponseEvent {}", response.request_guid.str()));
        return;
    }

    auto & [guid, mo] = *it;

    // TODO set price and volume from the response

    if (response.reject_reason.has_value() && !response.retry) {
        mo.ch.update([&](auto & order_ptr) {
            order_ptr->m_reject_reason = response.reject_reason.value();
        });
    }

    if (mo.traded) {
        m_orders.erase(it);
    }
    mo.acked = true;
}

bool OrderManager::try_trade_market_order(const TradeEvent & ev)
{
    const auto it = m_orders.find(ev.trade.order_guid());
    if (it == m_orders.end()) {
        return false;
    }

    auto & [guid, mo] = *it;

    mo.ch.update([&](std::shared_ptr<MarketOrder> & order_ptr) {
        order_ptr->on_trade(ev.trade.unsigned_volume(), ev.trade.price(), ev.trade.fee());
    });

    if (mo.ch.subscribers_count() == 0) {
        if (mo.acked) {
            m_orders.erase(it);
        }
    }
    mo.traded = true;

    return true;
}

bool OrderManager::try_trade_take_profit(const TradeEvent & ev)
{
    const auto it = m_take_profits.find(ev.trade.order_guid());
    if (it == m_take_profits.end()) {
        return false;
    }

    auto & [guid, channel] = *it;

    channel.update([&](std::shared_ptr<TakeProfitMarketOrder> & tp) {
        tp->on_trade(ev.trade.unsigned_volume(), ev.trade.price(), ev.trade.fee());
    });

    if (channel.subscribers_count() == 0) {
        m_take_profits.erase(it);
    }

    return true;
}

bool OrderManager::try_trade_stop_loss(const TradeEvent & ev)
{
    const auto it = m_stop_losses.find(ev.trade.order_guid());
    if (it == m_stop_losses.end()) {
        return false;
    }

    auto & [guid, channel] = *it;

    channel.update([&](std::shared_ptr<StopLossMarketOrder> & sl) {
        sl->on_trade(ev.trade.unsigned_volume(), ev.trade.price(), ev.trade.fee());
    });

    if (channel.subscribers_count() == 0) {
        m_stop_losses.erase(it);
    }

    return true;
}

void OrderManager::on_trade(const TradeEvent & ev)
{
    if (try_trade_market_order(ev)) {
        return;
    }

    if (try_trade_take_profit(ev)) {
        return;
    }

    if (try_trade_stop_loss(ev)) {
        return;
    }

    if (m_trailing_stop.has_value() && m_trailing_stop->get()->guid() == ev.trade.order_guid()) {
        return;
    }

    if (m_tpsl.has_value() && m_tpsl->get()->guid() == ev.trade.order_guid()) {
        return;
    }

    Logger::logf<LogLevel::Warning>("unsolicited TradeEvent {}", ev.trade.order_guid());
    m_error_channel.push(fmt::format("unsolicited TradeEvent {}", ev.trade.order_guid().str()));
}

EventObjectChannel<std::shared_ptr<TrailingStopLoss>> & OrderManager::send_trailing_stop(
        const TrailingStopLoss & trailing_stop,
        std::chrono::milliseconds)
{
    m_trailing_stop.emplace(EventObjectChannel<std::shared_ptr<TrailingStopLoss>>{});
    auto & order_channel = m_trailing_stop.value();

    order_channel.update([&](auto & tsl_sptr) {
        tsl_sptr = std::make_shared<TrailingStopLoss>(trailing_stop);
    });

    TrailingStopLossRequestEvent request(
            m_symbol,
            trailing_stop);

    m_tr_gateway.push_trailing_stop_request(request);

    return order_channel;
}

[[nodiscard("Subscribe for the channel")]]
EventObjectChannel<std::shared_ptr<TpslFullPos>> & OrderManager::send_tpsl(
        double take_profit_price,
        double stop_loss_price,
        Side side,
        std::chrono::milliseconds ts)
{
    auto & tpsl_ch = m_tpsl.emplace(EventObjectChannel<std::shared_ptr<TpslFullPos>>{});
    tpsl_ch.update([&](std::shared_ptr<TpslFullPos> & tpsl_sptr) {
        tpsl_sptr = std::make_shared<TpslFullPos>(
                m_symbol.symbol_name,
                take_profit_price,
                stop_loss_price,
                side,
                ts);
    });

    TpslRequestEvent req(
            m_symbol,
            take_profit_price,
            stop_loss_price);

    m_tr_gateway.push_tpsl_request(req);

    return tpsl_ch;
}

void OrderManager::on_trailing_stop_response(const TrailingStopLossUpdatedEvent & response)
{
    if (response.reject_reason.has_value()) {
        std::string err = "Rejected tpsl: " + response.reject_reason.value();
        m_error_channel.push(err);
        return;
    }

    if (!m_trailing_stop.has_value()) { // TODO check guid
        std::string err = "Unsolicited trailing stop response";
        m_error_channel.push(err);
        return;
    }

    if (response.stop_loss.has_value()) {
        m_trailing_stop->update([&](std::shared_ptr<TrailingStopLoss> & tsl) {
            tsl->m_active_stop_loss_price = response.stop_loss->stop_price();
            tsl->m_update_ts = response.timestamp;
        });
        return;
    }

    m_trailing_stop->update([&](std::shared_ptr<TrailingStopLoss> & tsl) {
        tsl = nullptr;
    });

    m_trailing_stop.reset();
    Logger::log<LogLevel::Debug>("Order manager: Trailing stop loss removed");
}

void OrderManager::on_tpsl_reposnse(const TpslUpdatedEvent & r)
{
    if (!m_tpsl.has_value()) {
        std::string err = "Unsolicited tpsl response";
        m_error_channel.push(err);
        return;
    }

    m_tpsl->update([&](std::shared_ptr<TpslFullPos> & tpsl_ptr) {
        tpsl_ptr->m_acked = true;

        tpsl_ptr->m_reject_reason = r.reject_reason;

        tpsl_ptr->m_set_up = r.set_up;
        tpsl_ptr->m_triggered = r.triggered;
    });

    if (!r.set_up && r.triggered) {
        m_tpsl.reset();
    }
}
