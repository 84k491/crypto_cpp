#include "ByBitTradingMessages.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace test {

TEST(BybitTradingMessagesTest, SimpleOrderExecution)
{
    const std::string msg_str = R"({"topic":"execution","id":"100475188_BTCUSDT_8935822012","creationTime":1708249213759,"data":[{"category":"linear","symbol":"BTCUSDT","closedSize":"0","execFee":"0.0855525","execId":"8262dbfa-5434-51ed-8458-3b97d3d49b3a","execPrice":"51850","execQty":"0.003","execType":"Trade","execValue":"155.55","feeRate":"0.00055","tradeIv":"","markIv":"","blockTradeId":"","markPrice":"51860.26","indexPrice":"","underlyingPrice":"","leavesQty":"0","orderId":"b49d6860-3062-4295-aa1f-c6471f3c9b20","orderLinkId":"1708249213491","orderPrice":"49257.6","orderQty":"0.003","orderType":"Market","stopOrderType":"UNKNOWN","side":"Sell","execTime":"1708249213755","isLeverage":"0","isMaker":false,"seq":8935822012,"marketUnit":"","createType":"CreateByUser"}]})";
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

TEST(BybitTradingMessagesTest, SimpleOrderAck)
{
     const std::string msg_str = R"({"topic":"order","id":"100475188_BTCUSDT_8935822012","creationTime":1708249213759,"data":[{"category":"linear","symbol":"BTCUSDT","orderId":"b49d6860-3062-4295-aa1f-c6471f3c9b20","orderLinkId":"1708249213491","blockTradeId":"","side":"Sell","positionIdx":0,"orderStatus":"Filled","cancelType":"UNKNOWN","rejectReason":"EC_NoError","timeInForce":"IOC","isLeverage":"","price":"49257.6","qty":"0.003","avgPrice":"51850","leavesQty":"0","leavesValue":"0","cumExecQty":"0.003","cumExecValue":"155.55","cumExecFee":"0.0855525","orderType":"Market","stopOrderType":"","orderIv":"","triggerPrice":"","takeProfit":"","stopLoss":"","triggerBy":"","tpTriggerBy":"","slTriggerBy":"","triggerDirection":0,"placeType":"","lastPriceOnCreated":"51850.1","closeOnTrigger":false,"reduceOnly":false,"smpGroup":0,"smpType":"None","smpOrderId":"","slLimitPrice":"0","tpLimitPrice":"0","tpslMode":"UNKNOWN","createType":"CreateByUser","marketUnit":"","createdTime":"1708249213755","updatedTime":"1708249213758","feeCurrency":""}]})";

    ByBitMessages::OrderResponseResult result;
    from_json(nlohmann::json::parse(msg_str), result);
}

TEST(BybitTradingMessagesTest, TrailingStopLossUpdate)
{
/*
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
    "topic":"order"}
*/
}

TEST(BybitTradingMessagesTest, TrailingStopLossFilled)
{
/*
[2024-09-28 12:47:32.167853629][Debug]: on_ws_message: {"creationTime":1727527652062,"data":[{"avgPrice":"64809.6","blockTradeId":"","cancelType":"UNKNOWN","category":
"linear","closeOnTrigger":false,"closedPnl":"0","createType":"CreateByUser","createdTime":"1727527652057","cumExecFee":"0.03564528","cumExecQty":"0.001","cumExecValue"
:"64.8096","feeCurrency":"","isLeverage":"","lastPriceOnCreated":"64617.3","leavesQty":"0","leavesValue":"0","marketUnit":"","orderId":"8df8fa97-734c-47b4-8fc8-da55b70
68011","orderIv":"","orderLinkId":"f8d63d27-d268-4d8d-b6f4-7d44142266fe","orderStatus":"Filled","orderType":"Market","placeType":"","positionIdx":0,"price":"67848.1","
qty":"0.001","reduceOnly":false,"rejectReason":"EC_NoError","side":"Buy","slLimitPrice":"0","slTriggerBy":"","smpGroup":0,"smpOrderId":"","smpType":"None","stopLoss":"
","stopOrderType":"","symbol":"BTCUSDT","takeProfit":"","timeInForce":"IOC","tpLimitPrice":"0","tpTriggerBy":"","tpslMode":"UNKNOWN","triggerBy":"","triggerDirection":
0,"triggerPrice":"","updatedTime":"1727527652061"}],"id":"100475188_BTCUSDT_9364011907","topic":"order"}
*/
}

TEST(BybitTradingMessagesTest, TrailingStopLossDeactivated)
{
/*
[2024-09-28 12:47:40.386458635][Debug]: on_ws_message: {"creationTime":1727527660281,"data":[{"avgPrice":"","blockTradeId":"","cancelType":"CancelByTpSlTsClear","categ
ory":"linear","closeOnTrigger":true,"closedPnl":"0","createType":"CreateByTrailingStop","createdTime":"1727527652277","cumExecFee":"0","cumExecQty":"0","cumExecValue":
"0","feeCurrency":"","isLeverage":"","lastPriceOnCreated":"64809.6","leavesQty":"0","leavesValue":"0","marketUnit":"","orderId":"cb755583-5cc2-474f-82b5-40a4148fa943",
"orderIv":"","orderLinkId":"","orderStatus":"Deactivated","orderType":"Market","placeType":"","positionIdx":0,"price":"0","qty":"0.001","reduceOnly":true,"rejectReason
":"EC_NoError","side":"Sell","slLimitPrice":"0","slTriggerBy":"","smpGroup":0,"smpOrderId":"","smpType":"None","stopLoss":"","stopOrderType":"TrailingStop","symbol":"B
TCUSDT","takeProfit":"","timeInForce":"IOC","tpLimitPrice":"0","tpTriggerBy":"","tpslMode":"Full","triggerBy":"LastPrice","triggerDirection":2,"triggerPrice":"64556.9"
,"updatedTime":"1727527660279"},{"avgPrice":"64627.3","blockTradeId":"","cancelType":"UNKNOWN","category":"linear","closeOnTrigger":false,"closedPnl":"-0.2534903","cre
ateType":"CreateByUser","createdTime":"1727527660275","cumExecFee":"0.03554502","cumExecQty":"0.001","cumExecValue":"64.6273","feeCurrency":"","isLeverage":"","lastPri
ceOnCreated":"64809.6","leavesQty":"0","leavesValue":"0","marketUnit":"","orderId":"c1447821-3ac3-4a77-aebc-3c9181dbdb82","orderIv":"","orderLinkId":"ded4d47f-6928-4a7
4-ab49-0c914b555f02","orderStatus":"Filled","orderType":"Market","placeType":"","positionIdx":0,"price":"61569.2","qty":"0.001","reduceOnly":false,"rejectReason":"EC_N
oError","side":"Sell","slLimitPrice":"0","slTriggerBy":"","smpGroup":0,"smpOrderId":"","smpType":"None","stopLoss":"","stopOrderType":"","symbol":"BTCUSDT","takeProfit
":"","timeInForce":"IOC","tpLimitPrice":"0","tpTriggerBy":"","tpslMode":"UNKNOWN","triggerBy":"","triggerDirection":0,"triggerPrice":"","updatedTime":"1727527660279"}]
,"id":"100475188_BTCUSDT_9364011948","topic":"order"}
*/
}


// TPSL ack with take profit
// TPSL update with take profit
// TPSL ack with trailing stop
// TPSL update with trailing stop

} // namespace test
