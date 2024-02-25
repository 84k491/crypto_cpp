#include "ByBitTradingMessages.h"
#include "Ohlc.h"

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
    j.at("rejectReason").get_to(order.rejectReason);
    j.at("timeInForce").get_to(order.timeInForce);
    order.price = std::stod(j.at("price").get<std::string>());
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

}
