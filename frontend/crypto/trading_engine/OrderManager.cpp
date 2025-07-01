#include "OrderManager.h"

#include "Logger.h"
#include "ScopeExit.h"

#include "fmt/format.h"

#include <cassert>
#include <stdexcept>
#include <utility>

OrderManager::OrderManager(
        Symbol symbol,
        EventLoopSubscriber & event_loop,
        ITradingGateway & tr_gateway)
    : m_symbol(std::move(symbol))
    , m_tr_gateway(tr_gateway)
    , m_event_loop(event_loop)
{
    m_event_loop.subscribe(
            m_tr_gateway.order_response_channel(),
            [this](const OrderResponseEvent & e) {
                on_order_response(e);
            });

    // there is no fees in order response
    m_event_loop.subscribe(
            m_tr_gateway.trade_channel(),
            [this](const TradeEvent & e) {
                on_order_trade(e);
            });

    m_event_loop.subscribe(
            m_tr_gateway.take_profit_update_channel(),
            [this](const TakeProfitUpdatedEvent & ev) {
                on_take_profit_response(ev);
            });

    m_event_loop.subscribe(
            m_tr_gateway.stop_loss_update_channel(),
            [this](const StopLossUpdatedEvent & ev) {
                on_stop_loss_reposnse(ev);
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
    }
    const auto adj_vol = std::get<SignedVolume>(adj_vol_var);

    const auto order = std::make_shared<MarketOrder>(
            m_symbol.symbol_name,
            price,
            adj_vol,
            ts);

    OrderRequestEvent or_event{*order};

    // TODO erase them
    const auto [it, success] = m_orders.emplace(
            order->guid(),
            EventObjectChannel<std::shared_ptr<MarketOrder>>{});
    assert(success);

    EventObjectChannel<std::shared_ptr<MarketOrder>> & order_channel = it->second;
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

    const auto [it, success] = m_pending_tp.emplace(
            tpmo->guid(),
            EventObjectChannel<std::shared_ptr<TakeProfitMarketOrder>>{});
    auto & ch = it->second;
    ch.update([&](auto & order_ptr) {
        order_ptr = tpmo;
    });
    ch.set_on_sub_count_changed([this, guid = tpmo->guid()](size_t sub_cnt) {
        if (sub_cnt == 0) {
            m_pending_tp.erase(guid);
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

    const auto [it, success] = m_pending_sl.emplace(
            slmo->guid(),
            EventObjectChannel<std::shared_ptr<StopLossMarketOrder>>{});
    auto & ch = it->second;
    ch.update([&](auto & order_ptr) {
        order_ptr = slmo;
    });
    ch.set_on_sub_count_changed([this, guid = slmo->guid()](size_t sub_cnt) {
        if (sub_cnt == 0) {
            m_pending_sl.erase(guid);
        }
    });

    m_tr_gateway.push_stop_loss_request(*slmo);

    return ch;
}

void OrderManager::cancel_take_profit(xg::Guid guid)
{
    m_tr_gateway.cancel_take_profit_request(guid);
}

void OrderManager::cancel_stop_loss(xg::Guid guid)
{
    m_tr_gateway.cancel_stop_loss_request(guid);
}

void OrderManager::on_take_profit_response(const TakeProfitUpdatedEvent & r)
{
    const auto it = m_pending_tp.find(r.guid);
    if (it == m_pending_tp.end()) {
        Logger::logf<LogLevel::Error>("Can't find tp on response: {}, active: {}", r.guid, r.active);
        return;
    }
    auto & ch = it->second;
    ch.update([&](std::shared_ptr<TakeProfitMarketOrder> & tp_ptr){
        tp_ptr->on_state_changed(r.active);
    });
}

void OrderManager::on_stop_loss_reposnse(const StopLossUpdatedEvent & r)
{
    const auto it = m_pending_sl.find(r.guid);
    if (it == m_pending_sl.end()) {
        Logger::logf<LogLevel::Error>("Can't find sl on response: {}, active: {}", r.guid, r.active);
        return;
    }
    auto & ch = it->second;
    ch.update([&](std::shared_ptr<StopLossMarketOrder> & sl_ptr){
        sl_ptr->on_state_changed(r.active);
    });
}

void OrderManager::on_order_response(const OrderResponseEvent & response)
{
    const auto it = m_orders.find(response.request_guid);
    if (it == m_orders.end()) {
        Logger::log<LogLevel::Warning>("unsolicited OrderResponseEvent");
        return;
    }
    ScopeExit se([&]() { m_orders.erase(it); });

    auto & [guid, channel] = *it;

    // TODO set price and volume from the response

    if (response.reject_reason.has_value() && !response.retry) {
        channel.update([&](auto & order_ptr) {
            order_ptr->m_reject_reason = response.reject_reason.value();
        });
    }

    // channel.update([&](auto & order_ptr) {
    //     order_ptr->;
    // });

    // if (!response.retry) {
    // }

    // {
    //     auto [old_guid, new_order] = *it; // copy
    //     new_order.first.regenerate_guid();
    //     OrderRequestEvent or_event{new_order.first};
    //     m_pending_orders.emplace(
    //             new_order.first.guid(),
    //             std::make_pair(
    //                     new_order.first,
    //                     std::move(new_order.second)));
    //     Logger::logf<LogLevel::Debug>("Re-sending order with a new guid: {} -> {}", old_guid, new_order.first.guid());
    //     m_tr_gateway.push_order_request(or_event);
    // }
}

void OrderManager::on_order_trade(const TradeEvent & ev)
{
    const auto it = m_orders.find(ev.trade.order_guid());
    if (it == m_orders.end()) {
        Logger::log<LogLevel::Warning>("unsolicited TradeEvent");
        return;
    }
    ScopeExit se([&]() { m_orders.erase(it); });

    auto & [guid, channel] = *it;

    channel.update([&](std::shared_ptr<MarketOrder> & order_ptr) {
        order_ptr->on_trade(ev.trade.unsigned_volume(), ev.trade.price(), ev.trade.fee());
    });

    if (channel.subscribers_count() == 0) {
        m_orders.erase(it);
    }
}
