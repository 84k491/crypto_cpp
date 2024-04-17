#include "ByBitTradingGateway.h"

#include "Events.h"
#include "Ohlc.h"
#include "ScopeExit.h"

#include <mutex>
#include <tuple>

ByBitTradingGateway::ByBitTradingGateway()
    : m_event_loop(*this)
    , m_url("wss://stream-testnet.bybit.com/v5/private")
    , m_api_key("HW1YEA13Q3msvHOMxx") // will expire at Jul 17, 2024
    , m_secret_key("Oi3u9iesDxzSN7aSmSsQKvBUG2Sp6A6Txzk1")
    , m_ws_client(
              m_url,
              {{m_api_key, m_secret_key}},
              [this](const json & j) { on_ws_message(j); })
{
    if (!m_ws_client.wait_until_ready()) {
        std::cout << "Failed to connect to ByBit trading" << std::endl;
        return;
    }

    m_ws_client.subscribe("order");
    m_ws_client.subscribe("execution");
}

void ByBitTradingGateway::on_order_response(const json & j)
{
    ByBitMessages::OrderResponseResult result;
    from_json(j, result);

    for (const auto & response : result.orders) {
        std::lock_guard l(m_pending_orders_mutex);
        const auto order_id_str = response.orderLinkId;
        const auto it = m_pending_orders.find(order_id_str);
        if (it != m_pending_orders.end()) {
            auto & [order_id, pending_order] = *it;
            pending_order.m_acked = true;

            if (pending_order.m_volume_to_fill <= 0) {
                auto & promise = pending_order.success_promise;
                promise.set_value(true);
                return;
            }
            std::cout << "Recevied ack for pending_order: " << order_id << "but it's not filled yet" << std::endl;
        }
        else {
            std::cout << "ERROR unsolicited order response" << std::endl;
        }
    }
}

void ByBitTradingGateway::on_execution(const json & j)
{
    std::cout << "Execution successfull " << std::endl;

    ByBitMessages::ExecutionResult result;
    from_json(j, result);

    for (const auto & response : result.executions) {
        std::lock_guard l(m_pending_orders_mutex);
        const auto it = m_pending_orders.find(response.orderLinkId);
        if (it != m_pending_orders.end()) {
            auto & [order_id, pending_order] = *it;
            std::cout << "Recevied execution of " << response.qty << " for order: " << order_id << std::endl;

            pending_order.m_volume_to_fill -= response.qty;
            auto trade_opt = response.to_trade();
            if (!trade_opt.has_value()) {
                std::cout << "ERROR can't get proper trade on execution: " << j << std::endl;
                return;
            }
            pending_order.m_trades.emplace_back(trade_opt.value());
            if (pending_order.m_volume_to_fill <= 0 && pending_order.m_acked) {
                auto & promise = pending_order.success_promise;
                promise.set_value(true);
                return;
            }
        }
        else {
            std::cout << "ERROR unsolicited execution" << std::endl;
        }
    }
}

void ByBitTradingGateway::push_order_request(const OrderRequestEvent & order)
{
    m_event_loop.as_consumer<OrderRequestEvent>().push(order);
}

void ByBitTradingGateway::push_tpsl_request(const TpslRequestEvent & tpsl_ev)
{
    m_event_loop.as_consumer<TpslRequestEvent>().push(tpsl_ev);
}

void ByBitTradingGateway::invoke(const std::variant<OrderRequestEvent, TpslRequestEvent> & variant)
{
    const auto * req_ptr = std::get_if<OrderRequestEvent>(&variant);
    if (req_ptr == nullptr) {
        std::cout << "ERROR: TPSL not implemented in ByBitTradingGateway!" << std::endl;
        return;
    }
    const auto & req = *req_ptr;
    const auto & order = req.order;

    int64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    const std::string order_id = std::to_string(ts);
    json json_order = {
            {"category", "linear"},
            {"symbol", order.symbol()},
            {"side", order.side_str()},
            {"orderType", "Market"},
            {"qty", std::to_string(order.volume().value())},
            {"timeInForce", "IOC"},
            {"orderLinkId", order_id},
            {"orderFilter", "Order"},
    };

    const auto request = json_order.dump();

    std::unique_lock l(m_pending_orders_mutex);
    const auto [it, success] = m_pending_orders.try_emplace(
            order_id,
            order);

    if (!success) {
        std::cout << "Trying to send order while order_id already exists: " << order_id << std::endl;
        req.reject_ev_consumer->push(OrderRejectedEvent(req.guid, true, "Duplicate order id", order));
        return;
    }
    ScopeExit se([&]() {
        std::lock_guard l(m_pending_orders_mutex);
        m_pending_orders.erase(it);
    });

    const std::string url = "https://api-testnet.bybit.com/v5/order/create";
    std::future<std::string> request_future = rest_client.request_auth_async(url, request, m_api_key, m_secret_key);
    request_future.wait();
    const std::string request_result = request_future.get();
    std::cout << "Enter order response: " << request_result << std::endl;

    {
        std::future<bool> success_future = it->second.success_promise.get_future();
        l.unlock();
        if (success_future.wait_for(ws_wait_timeout) == std::future_status::timeout) {
            std::cout << "Timeout waiting for order_resp" << std::endl;
            req.reject_ev_consumer->push(OrderRejectedEvent(req.guid, false, "Timeout waiting for order entry", order));
            return;
        }
        if (!success_future.get()) {
            std::cout << "Order failed" << order_id << std::endl;
            req.reject_ev_consumer->push(OrderRejectedEvent(req.guid, false, "Order failed to enter", order));
            return;
        }
    }
    std::cout << "Order successfully placed" << std::endl;
    req.event_consumer->push(OrderAcceptedEvent(req.guid, order));

    std::vector<Trade> trades = it->second.m_trades;
    for (auto & tr : trades) {
        req.trade_ev_consumer->push(TradeEvent(std::move(tr)));
    }
}

void ByBitTradingGateway::on_ws_message(const json & j)
{
    if (j.find("topic") != j.end()) {
        const auto & topic = j.at("topic");
        const std::map<std::string, std::function<void(const json &)>> topic_handlers = {
                {"order", [&](const json & j) { on_order_response(j); }},
                {"execution", [&](const json & j) { on_execution(j); }},
        };
        if (const auto it = topic_handlers.find(topic); it == topic_handlers.end()) {
            std::cout << "Unregistered topic: " << j.dump() << std::endl;
            return;
        }
        else {
            it->second(j);
        }
        return;
    }
    std::cout << "Unrecognized message: " << j.dump() << std::endl;
}
