#include "ByBitTradingMessages.h"

#include "Ohlc.h"

#include <set>

namespace ByBitMessages {

void from_json(const json & j, OrderResponse & order)
{
    j.at("category").get_to(order.category);
    j.at("symbol").get_to(order.symbol);
    j.at("orderId").get_to(order.orderId);
    j.at("orderLinkId").get_to(order.orderLinkId);
    j.at("side").get_to(order.side);
    j.at("orderStatus").get_to(order.orderStatus);
    j.at("cancelType").get_to(order.cancelType);
    j.at("createType").get_to(order.createType);
    j.at("rejectReason").get_to(order.rejectReason);
    j.at("timeInForce").get_to(order.timeInForce);
    order.price = std::stod(j.at("price").get<std::string>());
    if (const std::string triggerPriceStr = j.at("triggerPrice").get<std::string>(); !triggerPriceStr.empty()) {
        order.triggerPrice = std::stod(triggerPriceStr);
    }
    else {
        order.triggerPrice = std::nullopt;
    }
    order.qty = std::stod(j.at("qty").get<std::string>());
    order.leavesQty = std::stod(j.at("leavesQty").get<std::string>());
    order.cumExecQty = std::stod(j.at("cumExecQty").get<std::string>());
    order.cumExecFee = std::stod(j.at("cumExecFee").get<std::string>());
    j.at("orderType").get_to(order.orderType);
    j.at("updatedTime").get_to(order.updatedTime);
}

void from_json(const json & j, OrderResponseResult & order)
{
    j.at("id").get_to(order.id);
    j.at("creationTime").get_to(order.creationTime);

    for (const auto & item : j.at("data")) {
        OrderResponse order_response;
        item.get_to(order_response);
        order.orders.push_back(order_response);
    }
}

void from_json(const json & j, Execution & exec)
{
    j.at("category").get_to(exec.category);
    j.at("symbol").get_to(exec.symbol);
    j.at("orderId").get_to(exec.orderId);
    j.at("orderLinkId").get_to(exec.orderLinkId);
    j.at("side").get_to(exec.side);
    exec.execPrice = std::stod(j.at("execPrice").get<std::string>());
    exec.qty = std::stod(j.at("execQty").get<std::string>());
    exec.leavesQty = std::stod(j.at("leavesQty").get<std::string>());
    exec.execFee = std::stod(j.at("execFee").get<std::string>());
    j.at("execTime").get_to(exec.execTime);
}

void from_json(const json & j, ExecutionResult & exec_res)
{
    j.at("id").get_to(exec_res.id);
    j.at("creationTime").get_to(exec_res.creationTime);

    for (const auto & item : j.at("data")) {
        Execution order_response;
        item.get_to(order_response);
        exec_res.executions.push_back(order_response);
    }
}

void from_json(const json & j, TpslResult & tpsl_res)
{
    /*
        Tpsl response: {"retCode":0,"retMsg":"OK","result":{},"retExtInfo":{},"time":1713898946216}
        parsed json: {"result":{},"retCode":0,"retExtInfo":{},"retMsg":"OK","time":1713898946216}
    */
    // TODO make a unit test with empty result

    j.at("retCode").get_to(tpsl_res.ret_code);
    j.at("retMsg").get_to(tpsl_res.ret_msg);
    // j.at("result").get_to(tpsl_res.result);
    // j.at("retExtInfo").get_to(tpsl_res.ret_ext_info);
    size_t ts = j.at("time");
    tpsl_res.timestamp = std::chrono::milliseconds(ts);
}

std::optional<Trade> Execution::to_trade() const
{
    const auto volume_opt = UnsignedVolume::from(qty);
    if (!volume_opt.has_value()) {
        Logger::logf<LogLevel::Error>("Failed to create UnsignedVolume from qty: {}", qty);
        return std::nullopt;
    }

    return Trade(
            std::chrono::milliseconds(std::stoll(execTime)),
            symbol,
            execPrice,
            volume_opt.value(),
            side == "Buy" ? Side::buy() : Side::sell(),
            execFee);
}

std::optional<TpslUpdatedEvent> OrderResponseResult::on_tpsl_update(const std::array<ByBitMessages::OrderResponse, 2> & updates)
{
    const auto & response = updates[0];

    // TODO handle deactivation and validate
    return TpslUpdatedEvent(response.symbol, true);
}

std::optional<TrailingStopLossUpdatedEvent> OrderResponseResult::on_trailing_stop_update(const ByBitMessages::OrderResponse & response)
{
    if (!response.triggerPrice.has_value()) {
        Logger::logf<LogLevel::Error>(
                "No trigger price for trailing stop update");
        return {};
    }
    auto side = response.side == "Buy" ? Side::buy() : Side::sell();
    Symbol symbol;
    symbol.symbol_name = response.symbol;
    std::optional<StopLoss> sl = {};
    if (response.orderStatus != "Filled" && response.orderStatus != "Deactivated") {
        sl = {symbol, response.triggerPrice.value(), side};
    }
    TrailingStopLossUpdatedEvent tsl_ev{sl, std::chrono::milliseconds(std::stoll(response.updatedTime))};
    return tsl_ev;
}

std::optional<OrderResponseEvent> OrderResponseResult::on_order_response(const ByBitMessages::OrderResponse & response)
{
    if (response.rejectReason != "EC_NoError") {
        return OrderResponseEvent(
                response.symbol,
                xg::Guid(response.orderLinkId),
                response.rejectReason);
    }

    return OrderResponseEvent(response.symbol, xg::Guid(response.orderLinkId));
}

std::optional<std::vector<OrderResponseResult::EventVariant>> OrderResponseResult::to_events() const
{
    std::vector<OrderResponseResult::EventVariant> res;
    std::vector<ByBitMessages::OrderResponse> tpsl_updates;
    for (const ByBitMessages::OrderResponse & response : orders) {
        if (std::set<std::string>{"CreateByStopLoss", "CreateByTakeProfit"}.contains(response.createType)) {
            tpsl_updates.push_back(response);
            if (tpsl_updates.size() > 1) {
                const auto tpsl_opt = on_tpsl_update({tpsl_updates[0], tpsl_updates[1]});
                if (!tpsl_opt.has_value()) {
                    return {};
                }
                res.emplace_back(tpsl_opt.value());
            }
            continue;
        }

        if ("CreateByTrailingStop" == response.createType) {
            const auto tsl_opt = on_trailing_stop_update(response);
            if (!tsl_opt.has_value()) {
                return {};
            }
            res.emplace_back(tsl_opt.value());
            continue;
        }

        const auto order_opt = on_order_response(response);
        if (!order_opt.has_value()) {
            return {};
        }
        res.emplace_back(order_opt.value());
    }
    return res;
}

} // namespace ByBitMessages
