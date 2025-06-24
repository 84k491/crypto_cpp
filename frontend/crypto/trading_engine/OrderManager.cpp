#include "OrderManager.h"

#include "Logger.h"
#include "ScopeExit.h"

#include "fmt/format.h"

OrderManager::OrderManager(
        Symbol symbol,
        EventLoopSubscriber & event_loop,
        ITradingGateway & tr_gateway)
    : m_symbol(symbol)
    , m_tr_gateway(tr_gateway)
    , m_event_loop(event_loop)
{
    m_event_loop.subscribe(
            m_tr_gateway.order_response_channel(),
            [this](const OrderResponseEvent & e) {
                on_order_response(e);
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

void OrderManager::send_market_order(double price, SignedVolume vol, std::chrono::milliseconds ts, OrderCallback && on_response)
{
    const auto adj_vol_var = adjusted_volume(vol);
    if (std::holds_alternative<std::string>(adj_vol_var)) {
        m_error_channel.push(std::get<std::string>(adj_vol_var));
    }
    const auto adj_vol = std::get<SignedVolume>(adj_vol_var);

    const auto order = MarketOrder{
            m_symbol.symbol_name,
            price,
            adj_vol,
            ts};

    OrderRequestEvent or_event{order};
    m_pending_orders.emplace(
            order.guid(),
            std::make_pair(order,
                           std::move(on_response)));
    m_tr_gateway.push_order_request(or_event);
}

void OrderManager::send_take_profit(double price, SignedVolume vol, std::chrono::milliseconds ts, TakeProfitCallback && on_response)
{
    const auto adj_vol_var = adjusted_volume(vol);
    if (std::holds_alternative<std::string>(adj_vol_var)) {
        m_error_channel.push(std::get<std::string>(adj_vol_var));
    }
    const auto adj_vol = std::get<SignedVolume>(adj_vol_var);

    const auto [v, s] = adj_vol.as_unsigned_and_side();
    TakeProfitMarketOrder tpmo{
            m_symbol.symbol_name,
            price,
            v,
            s,
            ts};

    m_pending_tp.emplace(
            tpmo.guid(),
            std::make_pair(tpmo, on_response));

    // m_tr_gateway.push_take_profit_request(tpmo); // TODO
}

void OrderManager::on_take_profit_response(const TakeProfitUpdatedEvent & r)
{
    const auto it = m_pending_tp.find(r.guid);
    if (it == m_pending_tp.end()) {
        Logger::logf<LogLevel::Error>("Can't find tp on response: {}, active: {}", r.guid, r.active);
        return;
    }
    const auto & [tp, callback] = it->second;

    callback(tp, r.active);
}

void on_stop_loss_reposnse(const StopLossUpdatedEvent & r);

void OrderManager::on_order_response(const OrderResponseEvent & response)
{
    const auto it = m_pending_orders.find(response.request_guid);
    if (it == m_pending_orders.end()) {
        Logger::log<LogLevel::Warning>("unsolicited OrderResponseEvent");
        return;
    }
    ScopeExit se([&]() { m_pending_orders.erase(it); });

    const auto & [guid, order_and_cb] = *it;
    const auto & [order, cb] = order_and_cb;

    // TODO set price and volume from the response

    if (response.reject_reason.has_value() && !response.retry) {
        const std::string er = fmt::format("OrderRejected: {}", response.reject_reason.value());
        Logger::logf<LogLevel::Warning>(er);
        m_error_channel.push(er);

        cb(order, false);
    }

    if (!response.retry) {
        cb(order, true);
        return;
    }

    {
        auto [old_guid, new_order] = *it; // copy
        new_order.first.regenerate_guid();
        OrderRequestEvent or_event{new_order.first};
        m_pending_orders.emplace(
                new_order.first.guid(),
                std::make_pair(
                        new_order.first,
                        std::move(new_order.second)));
        Logger::logf<LogLevel::Debug>("Re-sending order with a new guid: {} -> {}", old_guid, new_order.first.guid());
        m_tr_gateway.push_order_request(or_event);
    }
}

const std::map<xg::Guid, std::pair<MarketOrder, OrderManager::OrderCallback>> & OrderManager::pending_orders() const
{
    return m_pending_orders;
}
