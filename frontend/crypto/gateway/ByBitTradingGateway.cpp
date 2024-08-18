#include "ByBitTradingGateway.h"

#include "Events.h"
#include "Logger.h"
#include "Macros.h"
#include "Ohlc.h"

#include <set>

ByBitTradingGateway::ByBitTradingGateway()
    : m_event_loop(*this)
    , m_url("wss://stream-testnet.bybit.com/v5/private")
    , m_api_key("EzxMilnOJadmXdhy7r") // will expire at Oct 20, 2024
    , m_secret_key("8J0teoEQbIuGf86F3zgKAStEyoIETkhidUTQ")
    , m_connection_watcher(*this)
{
    if (!reconnect_ws_client()) {
        Logger::log<LogLevel::Warning>("Failed to connect to ByBit trading");
    }
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
            Logger::logf<LogLevel::Warning>("Failed to find consumer for symbol: {}", response.symbol);
            continue;
        }
        auto & consumers = it->second.second;

        if (response.rejectReason == "EC_NoError") {
            auto & ack_consumer = consumers.order_ack_consumer;
            ack_consumer.push(OrderResponseEvent(xg::Guid(order_id_str)));
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
        Logger::logf<LogLevel::Warning>("Failed to find consumer for symbol: {}", response.symbol);
        return;
    }
    auto & consumers = it->second.second;
    consumers.tpsl_update_consumer.push(TpslUpdatedEvent(response.symbol, true));
}

void ByBitTradingGateway::on_execution(const json & j)
{
    Logger::logf<LogLevel::Info>("Execution received {}", j);

    ByBitMessages::ExecutionResult result;
    from_json(j, result);

    for (const auto & response : result.executions) {
        auto trade_opt = response.to_trade();
        if (!trade_opt.has_value()) {
            Logger::logf<LogLevel::Error>("ERROR can't get proper trade on execution: {}", j);
            return;
        }
        auto lref = m_consumers.lock();
        auto it = lref.get().find(response.symbol);
        if (it == lref.get().end()) {
            Logger::logf<LogLevel::Warning>("Failed to find consumer for symbol: {}", response.symbol);
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

void ByBitTradingGateway::invoke(const std::variant<OrderRequestEvent, TpslRequestEvent, PingCheckEvent> & variant)
{
    std::visit(
            VariantMatcher{
                    [&](const OrderRequestEvent & order) { process_event(order); },
                    [&](const TpslRequestEvent & tpsl) { process_event(tpsl); },
                    [&](const PingCheckEvent & ping) { process_event(ping); },
            },
            variant);
}

void ByBitTradingGateway::process_event(const OrderRequestEvent & req)
{
    using namespace std::chrono_literals;
    const auto & order = req.order;

    if (!check_consumers(order.symbol())) {
        UNWRAP_RET_VOID(consumer, req.response_consumer.lock());
        consumer.push(OrderResponseEvent(req.order.guid(), "No trade consumer for symbol"));
        return;
    }

    const std::string order_id = order.guid().str();
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
    std::future<std::string> request_future = rest_client.request_auth_async(
            url,
            request,
            m_api_key,
            m_secret_key,
            1000ms);
    const std::future_status status = request_future.wait_for(5000ms);
    if (status != std::future_status::ready) {
        // TODO specify guid
        UNWRAP_RET_VOID(consumer, req.response_consumer.lock());
        consumer.push(
                OrderResponseEvent(req.order.guid(),
                                   "Order request timeout"));
        return;
    }
    const std::string request_result = request_future.get();
    Logger::logf<LogLevel::Debug>("Enter order response: {}", request_result);
}

void ByBitTradingGateway::process_event(const TpslRequestEvent & tpsl)
{
    using namespace std::chrono_literals;
    Logger::logf<LogLevel::Debug>("Got TpslRequestEvent");

    if (!check_consumers(tpsl.symbol.symbol_name)) {
        Logger::logf<LogLevel::Warning>("No trade consumer for this symbol: {}", tpsl.symbol.symbol_name);
        UNWRAP_RET_VOID(consumer, tpsl.response_consumer.lock());
        consumer.push(
                TpslResponseEvent(
                        tpsl.guid,
                        tpsl.tpsl,
                        std::format("No consumer for this symbol: {}", tpsl.symbol.symbol_name)));
        return;
    }
    // TODO validate stop and take prices

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
    std::future<std::string> request_future = rest_client.request_auth_async(
            url,
            request,
            m_api_key,
            m_secret_key,
            1000ms);
    const std::future_status status = request_future.wait_for(5000ms);
    if (status != std::future_status::ready) {
        // TODO specify guid
        UNWRAP_RET_VOID(consumer, tpsl.response_consumer.lock());
        consumer.push(TpslResponseEvent(tpsl.guid, tpsl.tpsl, "Request timed out"));
        return;
    }
    const std::string request_result = request_future.get();
    Logger::logf<LogLevel::Debug>("Enter TPSL response: {}", request_result);
    const auto j = json::parse(request_result);
    const ByBitMessages::TpslResult result = j.get<ByBitMessages::TpslResult>();
    if (result.ret_code != 0) {
        UNWRAP_RET_VOID(consumer, tpsl.response_consumer.lock());
        consumer.push(TpslResponseEvent(tpsl.guid, tpsl.tpsl, result.ret_msg));
        return;
    }
    UNWRAP_RET_VOID(consumer, tpsl.response_consumer.lock());
    consumer.push(TpslResponseEvent(tpsl.guid, tpsl.tpsl));
}

void ByBitTradingGateway::process_event(const PingCheckEvent & ping_event)
{
    m_connection_watcher.handle_request(ping_event);
}

void ByBitTradingGateway::on_ws_message(const json & j)
{
    Logger::logf<LogLevel::Debug>("on_ws_message: {}", j.dump());
    if (j.find("topic") != j.end()) {
        const auto & topic = j.at("topic");
        const std::map<std::string, std::function<void(const json &)>> topic_handlers = {
                {"order", [&](const json & j) { on_order_response(j); }},
                {"execution", [&](const json & j) { on_execution(j); }},
        };
        if (const auto it = topic_handlers.find(topic); it == topic_handlers.end()) {
            Logger::logf<LogLevel::Warning>("Unregistered topic: {}", j.dump());
            return;
        }
        else {
            it->second(j);
        }
        return;
    }
    Logger::logf<LogLevel::Warning>("Unrecognized message: {}", j.dump());
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

bool ByBitTradingGateway::reconnect_ws_client()
{
    m_ws_client = std::make_shared<WebSocketClient>(
            m_url,
            std::make_optional(WsKeys{m_api_key, m_secret_key}),
            [this](const json & j) { on_ws_message(j); },
            m_connection_watcher);

    if (!m_ws_client->wait_until_ready()) {
        return false;
    }

    m_ws_client->subscribe("order");
    m_ws_client->subscribe("execution");

    auto weak_ptr = std::weak_ptr<IPingSender>(m_ws_client);
    m_connection_watcher.set_ping_sender(weak_ptr);
    m_event_loop.as_consumer<PingCheckEvent>().push_delayed(ws_ping_interval, PingCheckEvent{});
    return true;
}

void ByBitTradingGateway::on_connection_lost()
{
    Logger::log<LogLevel::Warning>("Connection lost on trading, reconnecting...");
    if (!reconnect_ws_client()) {
        Logger::log<LogLevel::Warning>("Failed to connect to ByBit trading");
    }
}

void ByBitTradingGateway::on_connection_verified()
{
    m_event_loop.as_consumer<PingCheckEvent>().push_delayed(ws_ping_interval, PingCheckEvent{});
}
