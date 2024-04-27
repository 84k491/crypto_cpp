#include "ByBitTradingGateway.h"

#include "Events.h"
#include "Ohlc.h"

#include <set>

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

    std::vector<ByBitMessages::OrderResponse> tpsl_updates;
    for (const ByBitMessages::OrderResponse & response : result.orders) {
        if (std::set<std::string>{"CreateByStopLoss", "CreateByTakeProfit"}.contains(response.createType)) {
            tpsl_updates.push_back(response);
            if (tpsl_updates.size() > 1) {
                on_tpsl_update({tpsl_updates[0], tpsl_updates[1]});
            }
            continue;
        }

        const auto order_id_str = response.orderLinkId;
        auto lref = m_consumers.lock();
        auto it = lref.get().find(response.symbol);
        if (it == lref.get().end()) {
            std::cout << "Failed to find consumer for symbol: " << response.symbol << std::endl;
            continue;
        }
        auto & consumers = it->second.second;

        if (response.rejectReason == "EC_NoError") {
            auto & ack_consumer = consumers.order_ack_consumer;
            ack_consumer.push(OrderResponseEvent(xg::Guid(order_id_str))); // TODO fill guid to request and from response
        }
        else {
            auto & ack_consumer = consumers.order_ack_consumer;
            ack_consumer.push(OrderResponseEvent(
                    xg::Guid(order_id_str),
                    response.rejectReason));
        }
    }
}

void ByBitTradingGateway::on_tpsl_update(const std::array<ByBitMessages::OrderResponse, 2> & updates)
{
    const auto & response = updates[0];

    auto lref = m_consumers.lock();
    auto it = lref.get().find(response.symbol);
    if (it == lref.get().end()) {
        std::cout << "Failed to find consumer for symbol: " << response.symbol << std::endl;
        return;
    }
    auto & consumers = it->second.second;
    consumers.tpsl_update_consumer.push(TpslUpdatedEvent(response.symbol, true));
}

void ByBitTradingGateway::on_execution(const json & j)
{
    std::cout << "Execution successfull " << std::endl;

    ByBitMessages::ExecutionResult result;
    from_json(j, result);

    for (const auto & response : result.executions) {
        auto trade_opt = response.to_trade();
        if (!trade_opt.has_value()) {
            std::cout << "ERROR can't get proper trade on execution: " << j << std::endl;
            return;
        }
        auto lref = m_consumers.lock();
        auto it = lref.get().find(response.symbol);
        if (it == lref.get().end()) {
            std::cout << "Failed to find consumer for symbol: " << response.symbol << std::endl;
            continue;
        }
        auto & consumers = it->second.second;
        consumers.trade_consumer.push(TradeEvent(std::move(trade_opt.value())));
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
    bool event_parsed = false;
    if (const auto * order_req = std::get_if<OrderRequestEvent>(&variant); order_req != nullptr) {
        process_event(*order_req);
        event_parsed = true;
    }
    else if (const auto * tpsl_req = std::get_if<TpslRequestEvent>(&variant); tpsl_req != nullptr) {
        process_event(*tpsl_req);
        event_parsed = true;
    }

    if (!event_parsed) {
        std::cout << "ERROR: Unknown event type" << std::endl;
    }
}

void ByBitTradingGateway::process_event(const OrderRequestEvent & req)
{
    const auto & order = req.order;

    if (!check_consumers(order.symbol())) {
        req.event_consumer->push(OrderResponseEvent(req.guid, "No trade consumer for this symbol: "));
        return;
    }

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

    const std::string url = std::string(s_rest_base_url) + "/v5/order/create";
    std::future<std::string> request_future = rest_client.request_auth_async(url, request, m_api_key, m_secret_key);
    request_future.wait();
    const std::string request_result = request_future.get();
    std::cout << "Enter order response: " << request_result << std::endl;
    // TODO check and reject right away
}

void ByBitTradingGateway::process_event(const TpslRequestEvent & tpsl)
{
    std::cout << "Got TpslRequestEvent" << std::endl;

    if (!check_consumers(tpsl.symbol.symbol_name)) {
        std::cout << "ERROR: No trade consumer for this symbol: " << tpsl.symbol.symbol_name << std::endl;
        // TODO specify symbol std::print/fmt::format?
        tpsl.event_consumer->push(TpslResponseEvent(tpsl.guid, tpsl.tpsl, "No consumer for this symbol"));
        return;
    }

    json json_order = {
            {"category", "linear"},
            {"symbol", tpsl.symbol.symbol_name},
            {"takeProfit", std::to_string(tpsl.tpsl.take_profit_price)},
            {"stopLoss", std::to_string(tpsl.tpsl.stop_loss_price)},
            {"tpTriggerBy", "LastPrice"},
            {"slTriggerBy", "LastPrice"},
            {"tpslMode", "Full"},
            {"tpOrderType", "Market"},
            {"slOrderType", "Market"},
    };
    const auto request = json_order.dump();

    const std::string url = std::string(s_rest_base_url) + "/v5/position/trading-stop";
    std::future<std::string> request_future = rest_client.request_auth_async(url, request, m_api_key, m_secret_key);
    request_future.wait();
    const std::string request_result = request_future.get();
    const auto j = json::parse(request_result);
    const ByBitMessages::TpslResult result = j.get<ByBitMessages::TpslResult>();
    tpsl.event_consumer->push(TpslResponseEvent(tpsl.guid, tpsl.tpsl));
}

void ByBitTradingGateway::on_ws_message(const json & j)
{
    std::cout << "on_ws_message: " << j.dump() << std::endl;
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

void ByBitTradingGateway::register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers)
{
    auto lref = m_consumers.lock();
    lref.get().emplace(symbol.symbol_name, std::make_pair(guid, consumers));
}

void ByBitTradingGateway::unregister_consumers(xg::Guid guid)
{
    auto lref = m_consumers.lock();
    for (auto it = lref.get().begin(), end = lref.get().end(); it != end; ++it) {
        if (it->second.first == guid) {
            lref.get().erase(it);
            return;
        }
    }
}

bool ByBitTradingGateway::check_consumers(const std::string & symbol)
{
    auto lref = m_consumers.lock();
    return lref.get().contains(symbol);
}
