#include "Ohlc.h"
#include "Trade.h"
#include "Volume.h"

#include <Logger.h>
#include <cstdint>
#include <list>
#include <string>

namespace ByBitMessages {

struct OrderResponse
{
    std::string category;
    std::string symbol;
    std::string orderId;
    std::string orderLinkId;
    std::string side;
    std::string orderStatus;  // ":"Filled",
    std::string createType;   // ":"UNKNOWN",
    std::string cancelType;   // ":"UNKNOWN",
    std::string rejectReason; // ":"EC_NoError",
    std::string timeInForce;  // ":"IOC",
    double price;             // ":"40323.7",
    double qty;               // ":"0.002",
    double leavesQty;         // ":"0",
    double cumExecQty;        // ":"0.002",
    double cumExecFee;        // ":"0.046684",
    std::optional<double> triggerPrice;      //"triggerPrice":"62591.7",
    std::string orderType;    // ":"Market",
    std::string updatedTime;  // ":"1705336167611",
};

struct OrderResponseResult
{
    std::string id;
    uint64_t creationTime;

    std::list<OrderResponse> orders;
};

struct Execution
{
    std::string category;
    std::string symbol;
    std::string orderId;
    std::string orderLinkId;
    std::string side;
    double execPrice;
    double qty;
    double leavesQty;
    double execFee;
    std::string execTime;

    std::optional<Trade> to_trade() const
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
};

struct ExecutionResult
{
    std::string id;
    uint64_t creationTime;

    std::list<Execution> executions;
};

/*
{"topic":"execution",
"id":"100475188_BTCUSDT_8884323101",
"creationTime":1705844609135,
"data":[{
    "category":"linear",
    "symbol":"BTCUSDT",
    "closedSize":"0.006",
    "execFee":"0.1380357",
    "execId":"7613417a-d7fe-54a5-98a4-cd1f4b552c3d",
    "execPrice":"41829",
    "execQty":"0.006",
    "execType":"Trade",
    "execValue":"250.974",
    "feeRate":"0.00055",
    "tradeIv":"",
    "markIv":"",
    "blockTradeId":"",
    "markPrice":"41826.44",
    "indexPrice":"",
    "underlyingPrice":"",
    "leavesQty":"0",
    "orderId":"f7d72e38-4961-4f1a-a712-872ed5c55676",
    "orderLinkId":"",
    "orderPrice":"43915.5",
    "orderQty":"0.006",
    "orderType":"Market",
    "stopOrderType":"UNKNOWN",
    "side":"Buy",
    "execTime":"1705844609131",
    "isLeverage":"0",
    "isMaker":false,
    "seq":8884323101,
    "marketUnit":"",
    "createType":"CreateByClosing"
}]}
*/

struct TpslResult
{
    int ret_code = -1;
    std::string ret_msg;
    std::string result;
    std::string ret_ext_info;
    std::chrono::milliseconds timestamp;

    bool ok() const { return ret_code == 0 && ret_msg == "OK"; }
};

void from_json(const json & j, OrderResponseResult & order);
void from_json(const json & j, OrderResponse & order);
void from_json(const json & j, Execution & exec);
void from_json(const json & j, ExecutionResult & exec_res);
void from_json(const json & j, TpslResult & tpsl_res);

} // namespace ByBitMessages
