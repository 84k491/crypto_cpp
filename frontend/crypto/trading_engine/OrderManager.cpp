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

void OrderManager::send_market_order(double price, SignedVolume vol, std::chrono::milliseconds ts, OrderCallback && on_response)
{
    const std::optional<SignedVolume> adjusted_target_volume_opt = m_symbol.get_qty_floored(vol);
    if (!adjusted_target_volume_opt.has_value()) {
        const std::string er = fmt::format(
                "ERROR can't get proper volume on open_or_move, target_absolute_volume = {}, qty_step = {}",
                vol.value(),
                m_symbol.lot_size_filter.qty_step);
        Logger::logf<LogLevel::Error>(er);
        m_error_channel.push(er);
    }
    const SignedVolume adjusted_target_volume = adjusted_target_volume_opt.value();

    if (0. == adjusted_target_volume.value()) {
        const std::string er = "Zero volume on market order send";
        Logger::logf<LogLevel::Error>(er);
        m_error_channel.push(er);
    }

    const auto order = MarketOrder{
            m_symbol.symbol_name,
            price,
            adjusted_target_volume,
            ts};

    OrderRequestEvent or_event{order};
    m_pending_orders.emplace(
            order.guid(),
            std::make_pair(order,
                           std::move(on_response)));
    m_tr_gateway.push_order_request(or_event);
}

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
