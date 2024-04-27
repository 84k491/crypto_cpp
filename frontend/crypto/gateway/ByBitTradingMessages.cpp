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
    j.at("createType").get_to(order.createType);
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

} // namespace ByBitMessages
