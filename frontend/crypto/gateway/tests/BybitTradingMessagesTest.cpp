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

} // namespace test
