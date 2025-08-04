#include "ByBitTradingGateway.h"

#include "Events.h"
#include "LogLevel.h"
#include "Logger.h"
#include "Ohlc.h"

#include <chrono>
#include <stdexcept>
#include <variant>

ByBitTradingGateway::ByBitTradingGateway()
    : m_connection_watcher(*this)
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

    register_subs();
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

    for (const auto & var : events) {
        std::visit(
                VariantMatcher{
                        [&](const OrderResponseEvent & event) {
                            m_order_response_channel.push(event);
                        },
                        [&](const TpslUpdatedEvent & event) {
                            m_tpsl_updated_channel.push(event);
                        },
                        [&](const TrailingStopLossUpdatedEvent & event) {
                            m_trailing_stop_update_channel.push(event);
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
        m_trade_channel.push(TradeEvent(std::move(trade_opt.value())));
    }
}

void ByBitTradingGateway::push_order_request(const OrderRequestEvent & order)
{
    m_order_req_channel.push(order);
}

void ByBitTradingGateway::push_tpsl_request(const TpslRequestEvent & tpsl_ev)
{
    m_tpsl_req_channel.push(tpsl_ev);
}

void ByBitTradingGateway::push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev)
{
    m_tsl_req_channel.push(trailing_stop_ev);
}

void ByBitTradingGateway::push_take_profit_request(const TakeProfitMarketOrder &)
{
    throw std::runtime_error{"not implemented"};
}

void ByBitTradingGateway::push_stop_loss_request(const StopLossMarketOrder &)
{
    throw std::runtime_error{"not implemented"};
}

void ByBitTradingGateway::cancel_stop_loss_request(xg::Guid)
{
    throw std::runtime_error{"not implemented"};
}

void ByBitTradingGateway::cancel_take_profit_request(xg::Guid)
{
    throw std::runtime_error{"not implemented"};
}

void ByBitTradingGateway::register_subs()
{
    m_event_loop.subscribe(
            m_order_req_channel,
            [this](const OrderRequestEvent & e) { process_event(e); });

    m_event_loop.subscribe(
            m_tpsl_req_channel,
            [this](const TpslRequestEvent & e) { process_event(e); });

    m_event_loop.subscribe(
            m_tsl_req_channel,
            [this](const TrailingStopLossRequestEvent & e) { process_event(e); });

    m_event_loop.subscribe(
            m_ping_event_channel,
            [this](const PingCheckEvent & e) { process_event(e); });
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
            {"qty", std::to_string(order.target_volume().value())},
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
        m_order_response_channel.push(nack_ev);
        return;
    }
    const std::string request_result = request_future.get();
    Logger::logf<LogLevel::Debug>("Enter order response: {}", request_result);

    if (!request_result.empty()) {
        auto event = OrderResponseEvent(
                req.order.symbol(),
                req.order.guid());
        m_order_response_channel.push(event);
        return;
    }

    Logger::logf<LogLevel::Warning>("Empty order response for order: {}. Need to push this request again with another guid", req.order.guid());
    auto event = OrderResponseEvent(
            req.order.symbol(),
            req.order.guid(),
            "Empty order response. Need to push this request again with other guid");
    event.retry = true;
    m_order_response_channel.push(event);
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
        m_tpsl_updated_channel.push(TpslUpdatedEvent{
                tpsl.symbol.symbol_name,
                tpsl.guid,
                false,
                false,
                "Request timed out"});
        return;
    }
    const std::string request_result = request_future.get();
    Logger::logf<LogLevel::Debug>("Enter TPSL response: {}", request_result);
    const auto j = json::parse(request_result);
    const ByBitMessages::TpslResult result = j.get<ByBitMessages::TpslResult>();
    if (result.ret_code != 0) {
        m_tpsl_updated_channel.push(TpslUpdatedEvent(
                tpsl.symbol.symbol_name,
                tpsl.guid,
                false,
                false,
                result.ret_msg));
        return;
    }
}

void ByBitTradingGateway::process_event(const TrailingStopLossRequestEvent & tsl)
{
    using namespace std::chrono_literals;

    Logger::logf<LogLevel::Debug>("Got TrailingStopLossRequestEvent");

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
        m_trailing_stop_update_channel.push(TrailingStopLossUpdatedEvent(
                tsl.symbol.symbol_name,
                {},
                {},
                "Request timed out"));
        return;
    }
    const std::string request_result = request_future.get();
    Logger::logf<LogLevel::Debug>("Bybit::TPSL response: {}", request_result);
    const auto j = json::parse(request_result);
    const ByBitMessages::TpslResult result = j.get<ByBitMessages::TpslResult>();
    if (result.ret_code != 0) {
        m_trailing_stop_update_channel.push(TrailingStopLossUpdatedEvent(
                tsl.symbol.symbol_name,
                {},
                {},
                result.ret_msg));
        return;
    }
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

bool ByBitTradingGateway::reconnect_ws_client()
{
    m_ws_client = std::make_shared<WebSocketClient>(
            m_config.ws_url,
            std::make_optional(WsKeys{.m_api_key = m_config.api_key, .m_secret_key = m_config.secret_key}),
            [this](const json & j) { on_ws_message(j); },
            m_connection_watcher);

    if (!m_ws_client->wait_until_ready()) {
        return false;
    }

    m_ws_client->subscribe("order");
    m_ws_client->subscribe("execution");

    auto weak_ptr = std::weak_ptr<IPingSender>(m_ws_client);
    m_connection_watcher.set_ping_sender(weak_ptr);
    m_ping_event_channel.push_delayed(PingCheckEvent{}, ws_ping_interval);
    return true;
}

void ByBitTradingGateway::on_connection_lost()
{
    Logger::log<LogLevel::Warning>("Connection lost on trading, reconnecting...");
    if (!reconnect_ws_client()) {
        Logger::log<LogLevel::Warning>("Failed to connect to ByBit trading");
        m_ping_event_channel.push_delayed(PingCheckEvent{}, std::chrono::seconds{30});
    }
}

void ByBitTradingGateway::on_connection_verified()
{
    m_ping_event_channel.push_delayed(PingCheckEvent{}, ws_ping_interval);
}

EventChannel<OrderResponseEvent> & ByBitTradingGateway::order_response_channel()
{
    return m_order_response_channel;
}

EventChannel<TradeEvent> & ByBitTradingGateway::trade_channel()
{
    return m_trade_channel;
}

EventChannel<TpslUpdatedEvent> & ByBitTradingGateway::tpsl_updated_channel()
{
    return m_tpsl_updated_channel;
}

EventChannel<TrailingStopLossUpdatedEvent> & ByBitTradingGateway::trailing_stop_update_channel()
{
    return m_trailing_stop_update_channel;
}

EventChannel<StopLossUpdatedEvent> & ByBitTradingGateway::stop_loss_update_channel()
{
    // TODO
    throw std::runtime_error("not implemented");
}

EventChannel<TakeProfitUpdatedEvent> & ByBitTradingGateway::take_profit_update_channel()
{
    throw std::runtime_error("not implemented");
}
