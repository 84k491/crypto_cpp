#include "ByBitTradingMessages.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace test {

TEST(BybitTradingMessagesTest, SimpleOrderExecution)
{
    const std::string msg_str = R"(
        {
            "topic":"execution",
            "id":"100475188_BTCUSDT_8935822012",
            "creationTime":1708249213759,
            "data":[
                {
                    "category":"linear",
                    "symbol":"BTCUSDT",
                    "closedSize":"0",
                    "execFee":"0.0855525",
                    "execId":"8262dbfa-5434-51ed-8458-3b97d3d49b3a",
                    "execPrice":"51850",
                    "execQty":"0.003",
                    "execType":"Trade",
                    "execValue":"155.55",
                    "feeRate":"0.00055",
                    "tradeIv":"",
                    "markIv":"",
                    "blockTradeId":"",
                    "markPrice":"51860.26",
                    "indexPrice":"",
                    "underlyingPrice":"",
                    "leavesQty":"0",
                    "orderId":"b49d6860-3062-4295-aa1f-c6471f3c9b20",
                    "orderLinkId":"1708249213491",
                    "orderPrice":"49257.6",
                    "orderQty":"0.003",
                    "orderType":"Market",
                    "stopOrderType":"UNKNOWN",
                    "side":"Sell",
                    "execTime":"1708249213755",
                    "isLeverage":"0",
                    "isMaker":false,
                    "seq":8935822012,
                    "marketUnit":"",
                    "createType":"CreateByUser"
                }
            ]
        })";
    const nlohmann::json jmsg = nlohmann::json::parse(msg_str);

    ByBitMessages::ExecutionResult result;
    from_json(jmsg, result);

    const auto & executions_list = result.executions;

    ASSERT_EQ(executions_list.size(), 1);

    const auto & exec = executions_list.front();

    ASSERT_EQ(exec.category, "linear");
    ASSERT_EQ(exec.symbol, "BTCUSDT");
    ASSERT_EQ(exec.orderId, "b49d6860-3062-4295-aa1f-c6471f3c9b20");
    ASSERT_EQ(exec.orderLinkId, "1708249213491");
    ASSERT_EQ(exec.side, "Sell");
    ASSERT_DOUBLE_EQ(exec.execPrice, 51850.0);
    ASSERT_DOUBLE_EQ(exec.qty, 0.003);
    ASSERT_DOUBLE_EQ(exec.leavesQty, 0.0);
    ASSERT_DOUBLE_EQ(exec.execFee, 0.0855525);
    ASSERT_EQ(exec.execTime, "1708249213755");

    const auto trade_opt = exec.to_trade();
    ASSERT_TRUE(trade_opt.has_value());
    const auto & trade = trade_opt.value();

    ASSERT_DOUBLE_EQ(trade.price(), 51850.0);
    ASSERT_DOUBLE_EQ(trade.unsigned_volume().value(), 0.003);
    ASSERT_DOUBLE_EQ(trade.fee(), 0.0855525);
    ASSERT_EQ(trade.ts().count(), 1708249213755);
    ASSERT_EQ(trade.symbol().symbol_name, "BTCUSDT");
    ASSERT_EQ(trade.side(), Side::sell());
}

TEST(BybitTradingMessagesTest, MarketOrderAck)
{
    const std::string msg_str = R"(
        {
            "topic":"order",
            "id":"100475188_BTCUSDT_8935822012",
            "creationTime":1708249213759,
            "data":[
                {
                    "category":"linear",
                    "symbol":"BTCUSDT",
                    "orderId":"b49d6860-3062-4295-aa1f-c6471f3c9b20",
                    "orderLinkId":"c49d6860-3062-4295-aa1f-c6471f3c9b20",
                    "blockTradeId":"",
                    "side":"Sell",
                    "positionIdx":0,
                    "orderStatus":"Filled",
                    "cancelType":"UNKNOWN",
                    "rejectReason":"EC_NoError",
                    "timeInForce":"IOC",
                    "isLeverage":"",
                    "price":"49257.6",
                    "qty":"0.003",
                    "avgPrice":"51850",
                    "leavesQty":"0",
                    "leavesValue":"0",
                    "cumExecQty":"0.003",
                    "cumExecValue":"155.55",
                    "cumExecFee":"0.0855525",
                    "orderType":"Market",
                    "stopOrderType":"",
                    "orderIv":"",
                    "triggerPrice":"",
                    "takeProfit":"",
                    "stopLoss":"",
                    "triggerBy":"",
                    "tpTriggerBy":"",
                    "slTriggerBy":"",
                    "triggerDirection":0,
                    "placeType":"",
                    "lastPriceOnCreated":"51850.1",
                    "closeOnTrigger":false,
                    "reduceOnly":false,
                    "smpGroup":0,
                    "smpType":"None",
                    "smpOrderId":"",
                    "slLimitPrice":"0",
                    "tpLimitPrice":"0",
                    "tpslMode":"UNKNOWN",
                    "createType":"CreateByUser",
                    "marketUnit":"",
                    "createdTime":"1708249213755",
                    "updatedTime":"1708249213758",
                    "feeCurrency":""
                }
            ]
        })";

    ByBitMessages::OrderResponseResult result;
    from_json(nlohmann::json::parse(msg_str), result);

    ASSERT_EQ(result.orders.size(), 1);
    const auto & order_response = result.orders.front();
    ASSERT_EQ(order_response.category, "linear");
    ASSERT_EQ(order_response.symbol, "BTCUSDT");
    ASSERT_EQ(order_response.orderId, "b49d6860-3062-4295-aa1f-c6471f3c9b20");
    ASSERT_EQ(order_response.price, 49257.6);
    ASSERT_EQ(order_response.qty, 0.003);
    ASSERT_EQ(order_response.leavesQty, 0.0);
    ASSERT_DOUBLE_EQ(order_response.cumExecQty, 0.003);
    ASSERT_DOUBLE_EQ(order_response.cumExecFee, 0.0855525);
    ASSERT_EQ(order_response.updatedTime, "1708249213758");

    const auto events_opt = result.to_events();
    ASSERT_TRUE(events_opt.has_value());
    const auto & events = events_opt.value();
    ASSERT_EQ(events.size(), 1);
    const auto & order_event_var = events.front();
    ASSERT_TRUE(std::holds_alternative<OrderResponseEvent>(order_event_var));
    const auto& order_event = std::get<OrderResponseEvent>(events.front());
    ASSERT_EQ(order_event.symbol_name, "BTCUSDT");
    ASSERT_EQ(order_event.request_guid, xg::Guid("c49d6860-3062-4295-aa1f-c6471f3c9b20"));
    ASSERT_EQ(order_event.reject_reason, std::nullopt);
}

TEST(BybitTradingMessagesTest, TrailingStopLossUpdate)
{
    const std::string msg_str = R"(
        {
            "creationTime":1727523073394,
            "data":[
                {
                    "avgPrice":"",
                    "blockTradeId":"",
                    "cancelType":"UNKNOWN",
                    "category":"linear",
                    "closeOnTrigger":true,
                    "closedPnl":"0",
                    "createType":"CreateByTrailingStop",
                    "createdTime":"1727522928543",
                    "cumExecFee":"0",
                    "cumExecQty":"0",
                    "cumExecValue":"0",
                    "feeCurrency":"",
                    "isLeverage":"",
                    "lastPriceOnCreated":"62927.6",
                    "leavesQty":"0.001",
                    "leavesValue":"0",
                    "marketUnit":"",
                    "orderId":"78d5779f-dbe8-4591-a206-4401bd310c90",
                    "orderIv":"",
                    "orderLinkId":"",
                    "orderStatus":"Untriggered",
                    "orderType":"Market",
                    "placeType":"",
                    "positionIdx":0,
                    "price":"0",
                    "qty":"0.001",
                    "reduceOnly":true,
                    "rejectReason":"EC_NoError",
                    "side":"Sell",
                    "slLimitPrice":"0",
                    "slTriggerBy":"",
                    "smpGroup":0,
                    "smpOrderId":"",
                    "smpType":"None",
                    "stopLoss":"",
                    "stopOrderType":"TrailingStop",
                    "symbol":"BTCUSDT",
                    "takeProfit":"",
                    "timeInForce":"IOC",
                    "tpLimitPrice":"0",
                    "tpTriggerBy":"",
                    "tpslMode":"UNKNOWN",
                    "triggerBy":"LastPrice",
                    "triggerDirection":2,
                    "triggerPrice":"62591.7",
                    "updatedTime":"1727523073392"
                }
            ],
            "id":"8d7f01d1-2457-4d20-b35c-d8691cc75dbb",
            "topic":"order"
        })";

    ByBitMessages::OrderResponseResult result;
    from_json(nlohmann::json::parse(msg_str), result);

    ASSERT_EQ(result.orders.size(), 1);
    const auto & order = result.orders.front();
    ASSERT_EQ(order.category, "linear");
    ASSERT_EQ(order.symbol, "BTCUSDT");
    ASSERT_EQ(order.orderId, "78d5779f-dbe8-4591-a206-4401bd310c90");
    ASSERT_EQ(order.price, 0.0);
    ASSERT_EQ(order.qty, 0.001);
    ASSERT_EQ(order.leavesQty, 0.001);
    ASSERT_EQ(order.orderStatus, "Untriggered");
    ASSERT_DOUBLE_EQ(order.cumExecQty, 0.0);
    ASSERT_DOUBLE_EQ(order.cumExecFee, 0.0);
    ASSERT_EQ(order.updatedTime, "1727523073392");

    const auto events_opt = result.to_events();
    ASSERT_TRUE(events_opt.has_value());
    const auto & events = events_opt.value();
    ASSERT_EQ(events.size(), 1);
    const auto & tsl_event_var = events.front();
    ASSERT_TRUE(std::holds_alternative<TrailingStopLossUpdatedEvent>(tsl_event_var));
    const auto& tsl_event = std::get<TrailingStopLossUpdatedEvent>(events.front());
    ASSERT_TRUE(tsl_event.stop_loss.has_value());
    ASSERT_EQ(tsl_event.symbol_name, "BTCUSDT");
    ASSERT_EQ(tsl_event.stop_loss->symbol().symbol_name, "BTCUSDT");
    ASSERT_EQ(tsl_event.stop_loss->stop_price(), 62591.7);
    ASSERT_EQ(tsl_event.stop_loss->side(), Side::sell());
}

TEST(BybitTradingMessagesTest, TrailingStopLossFilled)
{
    const std::string msg_str = R"(
        {
            "creationTime":1727528140038,
            "data":[
                {
                    "avgPrice":"64672.7",
                    "blockTradeId":"",
                    "cancelType":"UNKNOWN",
                    "category":"linear",
                    "closeOnTrigger":true,
                    "closedPnl":"-0.18480246",
                    "createType":"CreateByTrailingStop",
                    "createdTime":"1727528044959",
                    "cumExecFee":"0.03556999",
                    "cumExecQty":"0.001",
                    "cumExecValue":"64.6727",
                    "feeCurrency":"",
                    "isLeverage":"",
                    "lastPriceOnCreated":"64737.5",
                    "leavesQty":"0",
                    "leavesValue":"0",
                    "marketUnit":"",
                    "orderId":"7c5a170a-d562-4941-84a8-1252ec833bac",
                    "orderIv":"",
                    "orderLinkId":"",
                    "orderStatus":"Filled",
                    "orderType":"Market",
                    "placeType":"",
                    "positionIdx":0,
                    "price":"61500.7",
                    "qty":"0.001",
                    "reduceOnly":true,
                    "rejectReason":"EC_NoError",
                    "side":"Sell",
                    "slLimitPrice":"0",
                    "slTriggerBy":"",
                    "smpGroup":0,
                    "smpOrderId":"",
                    "smpType":"None",
                    "stopLoss":"",
                    "stopOrderType":"TrailingStop",
                    "symbol":"BTCUSDT",
                    "takeProfit":"",
                    "timeInForce":"IOC",
                    "tpLimitPrice":"0",
                    "tpTriggerBy":"",
                    "tpslMode":"UNKNOWN",
                    "triggerBy":"LastPrice",
                    "triggerDirection":2,
                    "triggerPrice":"64702.7",
                    "updatedTime":"1727528140037"
                }
            ],
            "id":"100475188_BTCUSDT_9364013701",
            "topic":"order"})";

    ByBitMessages::OrderResponseResult result;
    from_json(nlohmann::json::parse(msg_str), result);

    ASSERT_EQ(result.orders.size(), 1);
    const auto & order = result.orders.front();
    ASSERT_EQ(order.category, "linear");
    ASSERT_EQ(order.symbol, "BTCUSDT");
    ASSERT_EQ(order.orderId, "7c5a170a-d562-4941-84a8-1252ec833bac");
    ASSERT_EQ(order.price, 61500.7);
    ASSERT_EQ(order.triggerPrice, 64702.7);
    ASSERT_EQ(order.qty, 0.001);
    ASSERT_EQ(order.leavesQty, 0.0);
    ASSERT_EQ(order.orderStatus, "Filled");
    ASSERT_DOUBLE_EQ(order.cumExecQty, 0.001);
    ASSERT_DOUBLE_EQ(order.cumExecFee, 0.03556999);
    ASSERT_EQ(order.updatedTime, "1727528140037");

    const auto events_opt = result.to_events();
    ASSERT_TRUE(events_opt.has_value());
    const auto & events = events_opt.value();
    ASSERT_EQ(events.size(), 1);
    const auto & tsl_event_var = events.front();
    ASSERT_TRUE(std::holds_alternative<TrailingStopLossUpdatedEvent>(tsl_event_var));
    const auto& tsl_event = std::get<TrailingStopLossUpdatedEvent>(events.front());
    ASSERT_FALSE(tsl_event.stop_loss.has_value());
    ASSERT_EQ(tsl_event.symbol_name, "BTCUSDT");
}

TEST(BybitTradingMessagesTest, TrailingStopLossDeactivatedWithUserOrder)
{
    const std::string msg_str = R"(
        {
            "creationTime":1727527660281,
            "data":[
                {
                    "avgPrice":"",
                    "blockTradeId":"",
                    "cancelType":"CancelByTpSlTsClear",
                    "category":"linear",
                    "closeOnTrigger":true,
                    "closedPnl":"0",
                    "createType":"CreateByTrailingStop",
                    "createdTime":"1727527652277",
                    "cumExecFee":"0",
                    "cumExecQty":"0",
                    "cumExecValue":"0",
                    "feeCurrency":"",
                    "isLeverage":"",
                    "lastPriceOnCreated":"64809.6",
                    "leavesQty":"0",
                    "leavesValue":"0",
                    "marketUnit":"",
                    "orderId":"cb755583-5cc2-474f-82b5-40a4148fa943",
                    "orderIv":"",
                    "orderLinkId":"",
                    "orderStatus":"Deactivated",
                    "orderType":"Market",
                    "placeType":"",
                    "positionIdx":0,
                    "price":"0",
                    "qty":"0.001",
                    "reduceOnly":true,
                    "rejectReason":"EC_NoError",
                    "side":"Sell",
                    "slLimitPrice":"0",
                    "slTriggerBy":"",
                    "smpGroup":0,
                    "smpOrderId":"",
                    "smpType":"None",
                    "stopLoss":"",
                    "stopOrderType":"TrailingStop",
                    "symbol":"BTCUSDT",
                    "takeProfit":"",
                    "timeInForce":"IOC",
                    "tpLimitPrice":"0",
                    "tpTriggerBy":"",
                    "tpslMode":"Full",
                    "triggerBy":"LastPrice",
                    "triggerDirection":2,
                    "triggerPrice":"64556.9",
                    "updatedTime":"1727527660279"
                },
                {
                    "avgPrice":"64627.3",
                    "blockTradeId":"",
                    "cancelType":"UNKNOWN",
                    "category":"linear",
                    "closeOnTrigger":false,
                    "closedPnl":"-0.2534903",
                    "createType":"CreateByUser",
                    "createdTime":"1727527660275",
                    "cumExecFee":"0.03554502",
                    "cumExecQty":"0.001",
                    "cumExecValue":"64.6273",
                    "feeCurrency":"",
                    "isLeverage":"",
                    "lastPriceOnCreated":"64809.6",
                    "leavesQty":"0",
                    "leavesValue":"0",
                    "marketUnit":"",
                    "orderId":"c1447821-3ac3-4a77-aebc-3c9181dbdb82",
                    "orderIv":"",
                    "orderLinkId":"ded4d47f-6928-4a74-ab49-0c914b555f02",
                    "orderStatus":"Filled",
                    "orderType":"Market",
                    "placeType":"",
                    "positionIdx":0,
                    "price":"61569.2",
                    "qty":"0.001",
                    "reduceOnly":false,
                    "rejectReason":"EC_NoError",
                    "side":"Sell",
                    "slLimitPrice":"0",
                    "slTriggerBy":"",
                    "smpGroup":0,
                    "smpOrderId":"",
                    "smpType":"None",
                    "stopLoss":"",
                    "stopOrderType":"",
                    "symbol":"BTCUSDT",
                    "takeProfit":"",
                    "timeInForce":"IOC",
                    "tpLimitPrice":"0",
                    "tpTriggerBy":"",
                    "tpslMode":"UNKNOWN",
                    "triggerBy":"",
                    "triggerDirection":0,
                    "triggerPrice":"",
                    "updatedTime":"1727527660279"
                }
            ],
            "id":"100475188_BTCUSDT_9364011948",
            "topic":"order"})";

    ByBitMessages::OrderResponseResult result;
    from_json(nlohmann::json::parse(msg_str), result);

    ASSERT_EQ(result.orders.size(), 2);
    const auto & order = result.orders.front();

    ASSERT_EQ(order.orderId, "cb755583-5cc2-474f-82b5-40a4148fa943");
    ASSERT_EQ(order.orderStatus, "Deactivated");

    const auto & order2 = result.orders.back();
    ASSERT_EQ(order2.orderId, "c1447821-3ac3-4a77-aebc-3c9181dbdb82");
    ASSERT_EQ(order2.orderStatus, "Filled");

    // TODO check events
}

// TPSL ack with take profit
// TPSL update with take profit
// TPSL ack with trailing stop
// TPSL update with trailing stop

} // namespace test
