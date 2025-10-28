#include "Events.h"
#include "Ohlc.h"
#include "Trade.h"

#include <Logger.h>
#include <cstdint>
#include <list>
#include <string>
#include <variant>

namespace ByBitMessages {

struct OrderResponse
{
    std::string category;
    std::string symbol;
    std::string orderId;
    std::string orderLinkId;
    std::string side;
    std::string orderStatus;            // ":"Filled",
    std::string createType;             // ":"UNKNOWN",
    std::string cancelType;             // ":"UNKNOWN",
    std::string rejectReason;           // ":"EC_NoError",
    std::string timeInForce;            // ":"IOC",
    double price;                       // ":"40323.7",
    double qty;                         // ":"0.002",
    double leavesQty;                   // ":"0",
    double cumExecQty;                  // ":"0.002",
    double cumExecFee;                  // ":"0.046684",
    std::optional<double> triggerPrice; //"triggerPrice":"62591.7",
    std::string orderType;              // ":"Market",
    std::string stopOrderType;
    std::string updatedTime;            // ":"1705336167611",
};

struct OrderResponseResult
{
    using EventVariant = std::variant<OrderResponseEvent, TpslUpdatedEvent, TrailingStopLossUpdatedEvent>;

    std::string id;
    uint64_t creationTime;

    std::list<OrderResponse> orders;

    std::optional<std::vector<EventVariant>> to_events() const;

private:
    static std::optional<TpslUpdatedEvent> on_tpsl_update(const std::array<ByBitMessages::OrderResponse, 2> & updates);
    static std::optional<TrailingStopLossUpdatedEvent> on_trailing_stop_update(const ByBitMessages::OrderResponse & response);
    static std::optional<OrderResponseEvent> on_order_response(const ByBitMessages::OrderResponse & response);
};

struct Execution
{
    std::string category;
    std::string symbol;
    std::string orderId;
    std::string orderLinkId;
    std::string side;
    std::string execType;
    double execPrice;
    double qty;
    double leavesQty;
    double execFee;
    std::string execTime;

    std::optional<Trade> to_trade() const;
};

struct ExecutionResult
{
    std::string id;
    uint64_t creationTime;

    std::list<Execution> executions;
};

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
