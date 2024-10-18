#include "ByBitTradingGateway.h"

#include "Events.h"
#include "LogLevel.h"
#include "Logger.h"
#include "Macros.h"
#include "Ohlc.h"

#include <chrono>
#include <variant>

ByBitTradingGateway::ByBitTradingGateway()
    : m_connection_watcher(*this)
    , m_event_loop(*this)
{
    const auto config_opt = GatewayConfigLoader::load();
    if (!config_opt) {
        Logger::log<LogLevel::Error>("Failed to load bybit trading gateway config");
        return;
    }
    m_config = config_opt.value().trading;

    if (!reconnect_ws_client()) {
        Logger::log<LogLevel::Warning>("Failed to connect to ByBit trading");
    }
}

void ByBitTradingGateway::on_order_response(const json & j)
{
    ByBitMessages::OrderResponseResult result;
    from_json(j, result);

    const auto events_opt = result.to_events();
    if (!events_opt) {
        Logger::logf<LogLevel::Error>("Failed to parse order response: {}", j);
        return; // TODO panic
    }
    const auto & events = events_opt.value();

    auto lref = m_consumers.lock();
    for (const auto & var : events) {
        std::visit(
                VariantMatcher{
                        [&](const OrderResponseEvent & event) {
                            m_order_response_publisher.push(event);
                        },
                        [&](const TpslUpdatedEvent & event) {
                            m_tpsl_updated_publisher.push(event);
                        },
                        [&](const TrailingStopLossUpdatedEvent & event) {
                            auto it = lref.get().find(event.symbol_name);
                            if (it == lref.get().end()) {
                                Logger::logf<LogLevel::Warning>("Failed to find tpsl consumer for symbol: {}", event.symbol_name);
                                return;
                            }
                            auto & consumers = it->second.second;
                            consumers.trailing_stop_update_consumer.push(event);
                        }},
                var);
    }
}

void ByBitTradingGateway::on_execution(const json & j)
{
    Logger::logf<LogLevel::Info>("Execution received {}", j);

    ByBitMessages::ExecutionResult result;
    from_json(j, result);

    for (const auto & response : result.executions) {
        if (response.execType == "Funding") {
            // TODO handle funding fees
            Logger::logf<LogLevel::Info>("Execution is funding, skipping. Funding fee: {}", response.execFee);
            continue;
        }

        auto trade_opt = response.to_trade();
        if (!trade_opt.has_value()) {
            Logger::logf<LogLevel::Error>("ERROR can't get proper trade on execution: {}", j);
            return;
        }
        m_trade_publisher.push(TradeEvent(std::move(trade_opt.value())));
    }
}

void ByBitTradingGateway::push_order_request(const OrderRequestEvent & order)
{
    m_event_loop->as_consumer<OrderRequestEvent>().push(order);
}

void ByBitTradingGateway::push_tpsl_request(const TpslRequestEvent & tpsl_ev)
{
    m_event_loop->as_consumer<TpslRequestEvent>().push(tpsl_ev);
}

void ByBitTradingGateway::push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev)
{
    m_event_loop->as_consumer<TrailingStopLossRequestEvent>().push(trailing_stop_ev);
}

void ByBitTradingGateway::invoke(const std::variant<OrderRequestEvent, TpslRequestEvent, TrailingStopLossRequestEvent, PingCheckEvent> & variant)
{
    std::visit(
            VariantMatcher{
                    [&](const OrderRequestEvent & order) { process_event(order); },
                    [&](const TpslRequestEvent & tpsl) { process_event(tpsl); },
                    [&](const TrailingStopLossRequestEvent & tpsl) { process_event(tpsl); },
                    [&](const PingCheckEvent & ping) { process_event(ping); },
            },
            variant);
}

void ByBitTradingGateway::process_event(const OrderRequestEvent & req)
{
    using namespace std::chrono_literals;
    const auto & order = req.order;

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

    const std::string url = std::string(m_config.rest_url) + "/v5/order/create";
    std::future<std::string> request_future = rest_client.request_auth_async(
            url,
            request,
            m_config.api_key,
            m_config.secret_key,
            1000ms);
    const std::future_status status = request_future.wait_for(5000ms);
    if (status != std::future_status::ready) {
        const auto nack_ev = OrderResponseEvent(
                req.order.symbol(),
                req.order.guid(),
                "Order request timeout" + std::string{req.order.guid()});
        m_order_response_publisher.push(nack_ev);
        return;
    }
    const std::string request_result = request_future.get();
    Logger::logf<LogLevel::Debug>("Enter order response: {}", request_result);

    if (!request_result.empty()) {
        auto event = OrderResponseEvent(
                req.order.symbol(),
                req.order.guid());
        m_order_response_publisher.push(event);
        return;
    }

    Logger::logf<LogLevel::Warning>("Empty order response for order: {}. Need to push this request again with another guid", req.order.guid());
    auto event = OrderResponseEvent(
            req.order.symbol(),
            req.order.guid(),
            "Empty order response. Need to push this request again with other guid");
    event.retry = true;
    m_order_response_publisher.push(event);
}

void ByBitTradingGateway::process_event(const TpslRequestEvent & tpsl)
{
    using namespace std::chrono_literals;
    Logger::logf<LogLevel::Debug>("Got TpslRequestEvent");

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

    const std::string url = std::string(m_config.rest_url) + "/v5/position/trading-stop";
    std::future<std::string> request_future = rest_client.request_auth_async(
            url,
            request,
            m_config.api_key,
            m_config.secret_key,
            1000ms);
    const std::future_status status = request_future.wait_for(5000ms);
    if (status != std::future_status::ready) {
        // TODO specify guid
        m_tpsl_response_publisher.push(TpslResponseEvent(
                tpsl.symbol.symbol_name,
                tpsl.guid,
                tpsl.tpsl,
                "Request timed out"));
        return;
    }
    const std::string request_result = request_future.get();
    Logger::logf<LogLevel::Debug>("Enter TPSL response: {}", request_result);
    const auto j = json::parse(request_result);
    const ByBitMessages::TpslResult result = j.get<ByBitMessages::TpslResult>();
    if (result.ret_code != 0) {
        m_tpsl_response_publisher.push(TpslResponseEvent(
                tpsl.symbol.symbol_name,
                tpsl.guid,
                tpsl.tpsl,
                result.ret_msg));
        return;
    }
    m_tpsl_response_publisher.push(TpslResponseEvent(tpsl.symbol.symbol_name, tpsl.guid, tpsl.tpsl));
}

void ByBitTradingGateway::process_event(const TrailingStopLossRequestEvent & tsl)
{
    using namespace std::chrono_literals;

    Logger::logf<LogLevel::Debug>("Got TrailingStopLossRequestEvent");

    if (!check_consumers(tsl.symbol.symbol_name)) {
        Logger::logf<LogLevel::Warning>("No trade consumer for this symbol: {}", tsl.symbol.symbol_name);
        UNWRAP_RET_VOID(consumer, tsl.response_consumer.lock());
        consumer.push(
                TrailingStopLossResponseEvent(
                        tsl.guid,
                        tsl.trailing_stop_loss,
                        std::format("No consumer for this symbol: {}", tsl.symbol.symbol_name)));
        return;
    }
    // TODO validate stop price

    json json_order = {
            {"category", "linear"},
            {"symbol", tsl.symbol.symbol_name},
            // abs because it already knows the direction
            {"trailingStop", std::to_string(std::fabs(tsl.trailing_stop_loss.price_distance()))},
            {"activePrice", "0"},
            {"tpTriggerBy", "LastPrice"},
            {"slTriggerBy", "LastPrice"},
            {"tpslMode", "Full"},
            {"tpOrderType", "Market"},
            {"slOrderType", "Market"},
    };
    const auto request = json_order.dump();

    const std::string url = std::string(m_config.rest_url) + "/v5/position/trading-stop";
    std::future<std::string> request_future = rest_client.request_auth_async(
            url,
            request,
            m_config.api_key,
            m_config.secret_key,
            1000ms);
    const std::future_status status = request_future.wait_for(5000ms);
    if (status != std::future_status::ready) {
        // TODO specify guid
        UNWRAP_RET_VOID(consumer, tsl.response_consumer.lock());
        consumer.push(TrailingStopLossResponseEvent(tsl.guid, tsl.trailing_stop_loss, "Request timed out"));
        return;
    }
    const std::string request_result = request_future.get();
    Logger::logf<LogLevel::Debug>("Bybit::TPSL response: {}", request_result);
    const auto j = json::parse(request_result);
    const ByBitMessages::TpslResult result = j.get<ByBitMessages::TpslResult>();
    if (result.ret_code != 0) {
        UNWRAP_RET_VOID(consumer, tsl.response_consumer.lock());
        consumer.push(TrailingStopLossResponseEvent(tsl.guid, tsl.trailing_stop_loss, result.ret_msg));
        return;
    }
    UNWRAP_RET_VOID(consumer, tsl.response_consumer.lock());
    consumer.push(TrailingStopLossResponseEvent(tsl.guid, tsl.trailing_stop_loss));
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
            m_config.ws_url,
            std::make_optional(WsKeys{m_config.api_key, m_config.secret_key}),
            [this](const json & j) { on_ws_message(j); },
            m_connection_watcher);

    if (!m_ws_client->wait_until_ready()) {
        return false;
    }

    m_ws_client->subscribe("order");
    m_ws_client->subscribe("execution");

    auto weak_ptr = std::weak_ptr<IPingSender>(m_ws_client);
    m_connection_watcher.set_ping_sender(weak_ptr);
    m_event_loop->as_consumer<PingCheckEvent>().push_delayed(ws_ping_interval, PingCheckEvent{});
    return true;
}

void ByBitTradingGateway::on_connection_lost()
{
    Logger::log<LogLevel::Warning>("Connection lost on trading, reconnecting...");
    if (!reconnect_ws_client()) {
        Logger::log<LogLevel::Warning>("Failed to connect to ByBit trading");
        m_event_loop->as_consumer<PingCheckEvent>().push_delayed(std::chrono::seconds{30}, PingCheckEvent{});
    }
}

void ByBitTradingGateway::on_connection_verified()
{
    m_event_loop->as_consumer<PingCheckEvent>().push_delayed(ws_ping_interval, PingCheckEvent{});
}

EventPublisher<OrderResponseEvent> & ByBitTradingGateway::order_response_publisher()
{
    return m_order_response_publisher;
}

EventPublisher<TradeEvent> & ByBitTradingGateway::trade_publisher()
{
    return m_trade_publisher;
}

EventPublisher<TpslResponseEvent> & ByBitTradingGateway::tpsl_response_publisher()
{
    return m_tpsl_response_publisher;
}

EventPublisher<TpslUpdatedEvent> & ByBitTradingGateway::tpsl_updated_publisher()
{
    return m_tpsl_updated_publisher;
}
