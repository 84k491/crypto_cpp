#include "BacktestTradingGateway.h"

#include "ConditionalOrders.h"
#include "EventLoop.h"
#include "EventTimeseriesChannel.h"
#include "Events.h"
#include "crossguid/guid.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

namespace test {
using namespace testing;

class MockEventLoop : public ILambdaAcceptor
{
    void push(LambdaEvent value) override
    {
        value.func();
    }

    void push_delayed(std::chrono::milliseconds, LambdaEvent) override
    {
        throw std::runtime_error("Not implemented");
    }
};

class BacktestTradingGatewayTest : public Test
{
public:
    BacktestTradingGatewayTest() = default;

protected:
    EventTimeseriesChannel<double> price_source_ch;
    BacktestTradingGateway trgw;

    MockEventLoop el;
};

// push some price
// open pos
// push some more prices
// close pos
TEST_F(BacktestTradingGatewayTest, MarketOrderOpenClosePos)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    {
        MarketOrder mo{
                "TSTUSDT",
                111.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        bool order_responded = false;
        EventSubcriber order_sub{el};
        order_sub.subscribe(trgw.order_response_channel(),
                            [&](const OrderResponseEvent & r) {
                                order_responded = true;
                                EXPECT_EQ(r.request_guid, mo.guid());
                                EXPECT_FALSE(r.reject_reason.has_value());
                                EXPECT_FALSE(r.retry);
                            });

        bool trade_responded = false;
        EventSubcriber trade_sub{el};
        trade_sub.subscribe(trgw.trade_channel(),
                            [&](const TradeEvent & r) {
                                trade_responded = true;
                                EXPECT_EQ(r.trade.price(), 100.);
                                EXPECT_TRUE(r.trade.fee() > 0.);
                            });

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);

        EXPECT_TRUE(order_responded);
        EXPECT_TRUE(trade_responded);

        ASSERT_EQ(trgw.pos_volume().value(), 1);
    }

    price_source_ch.push(std::chrono::milliseconds{10}, 111.);
    price_source_ch.push(std::chrono::milliseconds{20}, 222.);

    {
        MarketOrder mo{
                "TSTUSDT",
                11.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{20},
        };

        bool order_responded = false;
        EventSubcriber order_sub{el};
        order_sub.subscribe(
                trgw.order_response_channel(),
                [&](const OrderResponseEvent & r) {
                    order_responded = true;
                    EXPECT_EQ(r.request_guid, mo.guid());
                    EXPECT_FALSE(r.reject_reason.has_value());
                    EXPECT_FALSE(r.retry);
                });

        bool trade_responded = false;
        EventSubcriber trade_sub{el};
        trade_sub.subscribe(
                trgw.trade_channel(),
                [&](const TradeEvent & r) {
                    trade_responded = true;
                    EXPECT_EQ(r.trade.price(), 222.);
                    EXPECT_TRUE(r.trade.fee() > 0.);
                });

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);

        EXPECT_TRUE(order_responded);
        EXPECT_TRUE(trade_responded);
        ASSERT_EQ(trgw.pos_volume().value(), 0);
    }
}

TEST_F(BacktestTradingGatewayTest, TpslRejectIfNoPos)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    bool tpsl_ack_responded = false;
    EventSubcriber tpsl_ack_sub{el};
    tpsl_ack_sub.subscribe(
            trgw.tpsl_updated_channel(),
            [&](const TpslUpdatedEvent & ev) {
                tpsl_ack_responded = true;
                EXPECT_EQ(ev.reject_reason, "can not set tp/sl/ts for zero position");
            });

    TpslFullPos::Prices tpsl{.take_profit_price = 130., .stop_loss_price = 10.};
    Symbol test_symbol{.symbol_name = "TSTUSDT", .lot_size_filter = {.min_qty = 0.1, .max_qty = 10., .qty_step = 0.1}};
    TpslRequestEvent ev{test_symbol, tpsl.take_profit_price, tpsl.stop_loss_price};

    ASSERT_EQ(trgw.pos_volume().value(), 0);

    trgw.push_tpsl_request(ev);

    ASSERT_TRUE(tpsl_ack_responded);

    ASSERT_EQ(trgw.pos_volume().value(), 0);
}

TEST_F(BacktestTradingGatewayTest, TpslTriggerTp)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    // open pos
    {
        MarketOrder mo{
                "TSTUSDT",
                111.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);
    }
    ASSERT_EQ(trgw.pos_volume().value(), 1);

    bool tpsl_ack_responded = false;
    std::optional<TpslUpdatedEvent> tpsl_upd_response;
    constexpr int tp_price = 120;

    EventSubcriber tpsl_upd_sub{el};
    tpsl_upd_sub.subscribe(
            trgw.tpsl_updated_channel(),
            [&](const TpslUpdatedEvent & ev) {
                tpsl_ack_responded = true;
                tpsl_upd_response = ev;
                EXPECT_FALSE(ev.reject_reason.has_value());
            });

    bool tp_trade_responded = false;

    EventSubcriber tp_trade_sub{el};
    tp_trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                tp_trade_responded = true;
                EXPECT_EQ(ev.trade.price(), tp_price);
            });

    TpslFullPos::Prices tpsl{.take_profit_price = tp_price, .stop_loss_price = 50.};
    Symbol test_symbol{.symbol_name = "TSTUSDT", .lot_size_filter = {.min_qty = 0.1, .max_qty = 10., .qty_step = 0.1}};
    TpslRequestEvent ev{test_symbol, tpsl.take_profit_price, tpsl.stop_loss_price};
    trgw.push_tpsl_request(ev);

    EXPECT_TRUE(tpsl_ack_responded);
    ASSERT_TRUE(tpsl_upd_response.has_value());
    EXPECT_TRUE(tpsl_upd_response->set_up);
    EXPECT_FALSE(tp_trade_responded);
    tpsl_upd_response = {};

    // triggering TP
    price_source_ch.push(std::chrono::milliseconds{2}, 130.);

    ASSERT_TRUE(tpsl_upd_response.has_value());
    EXPECT_FALSE(tpsl_upd_response->set_up);
    EXPECT_TRUE(tp_trade_responded);

    ASSERT_EQ(trgw.pos_volume().value(), 0);
}

TEST_F(BacktestTradingGatewayTest, TpslTriggerSl)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    // open pos
    {
        MarketOrder mo{
                "TSTUSDT",
                111.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);
    }
    ASSERT_EQ(trgw.pos_volume().value(), 1);

    bool tpsl_ack_responded = false;
    std::optional<TpslUpdatedEvent> tpsl_upd_response;
    constexpr int sl_price = 120;

    EventSubcriber tpsl_upd_sub{el};
    tpsl_upd_sub.subscribe(
            trgw.tpsl_updated_channel(),
            [&](const TpslUpdatedEvent & ev) {
                tpsl_ack_responded = true;
                tpsl_upd_response = ev;
                EXPECT_FALSE(ev.reject_reason.has_value());
            });

    bool tp_trade_responded = false;

    EventSubcriber tp_trade_sub{el};
    tp_trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                tp_trade_responded = true;
                EXPECT_EQ(ev.trade.price(), sl_price);
            });

    TpslFullPos::Prices tpsl{.take_profit_price = 130., .stop_loss_price = sl_price};
    Symbol test_symbol{.symbol_name = "TSTUSDT", .lot_size_filter = {.min_qty = 0.1, .max_qty = 10., .qty_step = 0.1}};
    TpslRequestEvent ev{test_symbol, tpsl.take_profit_price, tpsl.stop_loss_price};
    trgw.push_tpsl_request(ev);

    EXPECT_TRUE(tpsl_ack_responded);
    ASSERT_TRUE(tpsl_upd_response);
    EXPECT_TRUE(tpsl_upd_response->set_up);
    EXPECT_FALSE(tp_trade_responded);
    tpsl_upd_response = {};

    // triggering SL
    price_source_ch.push(std::chrono::milliseconds{2}, 40.);

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    EXPECT_TRUE(tpsl_upd_response);
    EXPECT_FALSE(tpsl_upd_response->set_up);
    EXPECT_TRUE(tp_trade_responded);
}

TEST_F(BacktestTradingGatewayTest, TpslRemoveOnClosePos)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    {
        MarketOrder mo{
                "TSTUSDT",
                111.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        OrderRequestEvent ev{mo};
        trgw.push_order_request(ev);
        ASSERT_EQ(trgw.pos_volume().value(), 1);
    }

    price_source_ch.push(std::chrono::milliseconds{20}, 222.);

    std::optional<TpslUpdatedEvent> tpsl_upd_response;
    constexpr int sl_price = 120;

    EventSubcriber tpsl_upd_sub{el};
    tpsl_upd_sub.subscribe(
            trgw.tpsl_updated_channel(),
            [&](const TpslUpdatedEvent & ev) {
                tpsl_upd_response = ev;
            });

    TpslFullPos::Prices tpsl{.take_profit_price = 130., .stop_loss_price = sl_price};
    Symbol test_symbol{.symbol_name = "TSTUSDT", .lot_size_filter = {.min_qty = 0.1, .max_qty = 10., .qty_step = 0.1}};
    TpslRequestEvent ev{test_symbol, tpsl.take_profit_price, tpsl.stop_loss_price};
    trgw.push_tpsl_request(ev);

    ASSERT_TRUE(tpsl_upd_response);
    EXPECT_TRUE(tpsl_upd_response->set_up);
    tpsl_upd_response = {};

    {
        MarketOrder mo{
                "TSTUSDT",
                11.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{20},
        };

        OrderRequestEvent or_ev{mo};
        trgw.push_order_request(or_ev);
        ASSERT_EQ(trgw.pos_volume().value(), 0);
    }

    ASSERT_TRUE(tpsl_upd_response);
    EXPECT_FALSE(tpsl_upd_response->set_up);
    ASSERT_EQ(trgw.pos_volume().value(), 0);
}

TEST_F(BacktestTradingGatewayTest, StopLossSellTriggerLater)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    std::optional<StopLossUpdatedEvent> sl_updated_response;

    EventSubcriber sl_updated_sub{el};
    sl_updated_sub.subscribe(
            trgw.stop_loss_update_channel(),
            [&sl_updated_response](const StopLossUpdatedEvent ev) {
                sl_updated_response = ev;
            });

    std::optional<TradeEvent> trade_msg;
    EventSubcriber trade_sub{el};
    trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                trade_msg = ev;
            });

    {
        StopLossMarketOrder sl{
                "TSTUSDT",
                90.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{1},
        };

        trgw.push_stop_loss_request(sl);
        ASSERT_EQ(trgw.pos_volume().value(), 0);
    }

    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_TRUE(sl_updated_response->active);
    sl_updated_response.reset();
    ASSERT_FALSE(trade_msg.has_value());

    // this must not trigger sell stop loss
    price_source_ch.push(std::chrono::milliseconds{20}, 222.);

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_FALSE(sl_updated_response.has_value());
    ASSERT_FALSE(trade_msg.has_value());

    // exact price, must trigger and open pos
    price_source_ch.push(std::chrono::milliseconds{30}, 90.);

    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_FALSE(sl_updated_response->active);
    ASSERT_TRUE(trade_msg.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), -1);
}

TEST_F(BacktestTradingGatewayTest, StopLossBuyTriggerLater)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 90.);

    std::optional<StopLossUpdatedEvent> sl_updated_response;

    EventSubcriber sl_updated_sub{el};
    sl_updated_sub.subscribe(
            trgw.stop_loss_update_channel(),
            [&sl_updated_response](const StopLossUpdatedEvent ev) {
                sl_updated_response = ev;
            });

    std::optional<TradeEvent> trade_msg;
    EventSubcriber trade_sub{el};
    trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                trade_msg = ev;
            });

    {
        StopLossMarketOrder sl{
                "TSTUSDT",
                100.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        trgw.push_stop_loss_request(sl);
    }

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_TRUE(sl_updated_response->active);
    sl_updated_response.reset();
    ASSERT_FALSE(trade_msg.has_value());

    // this must not trigger sell stop loss
    price_source_ch.push(std::chrono::milliseconds{20}, 88.);

    ASSERT_FALSE(sl_updated_response.has_value());
    ASSERT_FALSE(trade_msg.has_value());

    // exact price, must trigger
    price_source_ch.push(std::chrono::milliseconds{30}, 100.);

    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_FALSE(sl_updated_response->active);
    ASSERT_TRUE(trade_msg.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), 1);
}

TEST_F(BacktestTradingGatewayTest, TakeProfitSellTriggerLater)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    std::optional<TakeProfitUpdatedEvent> sl_updated_response;
    EventSubcriber sl_updated_sub{el};
    sl_updated_sub.subscribe(
            trgw.take_profit_update_channel(),
            [&sl_updated_response](const TakeProfitUpdatedEvent ev) {
                sl_updated_response = ev;
            });

    std::optional<TradeEvent> trade_msg;
    EventSubcriber trade_sub{el};
    trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                trade_msg = ev;
            });

    {
        TakeProfitMarketOrder tp{
                "TSTUSDT",
                110.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{1},
        };

        trgw.push_take_profit_request(tp);
    }

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_TRUE(sl_updated_response->active);
    sl_updated_response.reset();
    ASSERT_FALSE(trade_msg.has_value());

    // this must not trigger sell stop loss
    price_source_ch.push(std::chrono::milliseconds{20}, 90.);

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_FALSE(sl_updated_response.has_value());
    ASSERT_FALSE(trade_msg.has_value());

    // exact price, must trigger
    price_source_ch.push(std::chrono::milliseconds{30}, 110.);

    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_FALSE(sl_updated_response->active);
    ASSERT_TRUE(trade_msg.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), -1);
}

TEST_F(BacktestTradingGatewayTest, TakeProfitBuyTriggerLater)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    std::optional<TakeProfitUpdatedEvent> sl_updated_response;
    EventSubcriber sl_updated_sub{el};
    sl_updated_sub.subscribe(
            trgw.take_profit_update_channel(),
            [&sl_updated_response](const TakeProfitUpdatedEvent ev) {
                sl_updated_response = ev;
            });

    std::optional<TradeEvent> trade_msg;
    EventSubcriber trade_sub{el};
    trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                trade_msg = ev;
            });

    {
        TakeProfitMarketOrder tp{
                "TSTUSDT",
                90.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        trgw.push_take_profit_request(tp);
    }

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_TRUE(sl_updated_response->active);
    sl_updated_response.reset();
    ASSERT_FALSE(trade_msg.has_value());

    // this must not trigger sell stop loss
    price_source_ch.push(std::chrono::milliseconds{20}, 110.);

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_FALSE(sl_updated_response.has_value());
    ASSERT_FALSE(trade_msg.has_value());

    // exact price, must trigger
    price_source_ch.push(std::chrono::milliseconds{30}, 90.);

    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_FALSE(sl_updated_response->active);
    ASSERT_TRUE(trade_msg.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), 1);
}

TEST_F(BacktestTradingGatewayTest, StopLossCancel)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    std::optional<StopLossUpdatedEvent> sl_updated_response;
    EventSubcriber sl_updated_sub{el};
    sl_updated_sub.subscribe(
            trgw.stop_loss_update_channel(),
            [&sl_updated_response](const StopLossUpdatedEvent ev) {
                sl_updated_response = ev;
            });

    xg::Guid guid;
    {
        StopLossMarketOrder sl{
                "TSTUSDT",
                90.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{1},
        };

        guid = sl.guid();
        trgw.push_stop_loss_request(sl);
        ASSERT_EQ(trgw.pos_volume().value(), 0);
    }

    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_TRUE(sl_updated_response->active);
    sl_updated_response.reset();

    // this must not trigger sell stop loss
    price_source_ch.push(std::chrono::milliseconds{20}, 222.);

    {
        trgw.cancel_stop_loss_request(guid);
    }

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_FALSE(sl_updated_response->active);
    sl_updated_response.reset();

    // exact price, must trigger and open pos
    price_source_ch.push(std::chrono::milliseconds{30}, 90.);

    ASSERT_FALSE(sl_updated_response.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), 0);
}

TEST_F(BacktestTradingGatewayTest, TakeProfitCancel)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    std::optional<TakeProfitUpdatedEvent> sl_updated_response;
    EventSubcriber sl_updated_sub{el};
    sl_updated_sub.subscribe(
            trgw.take_profit_update_channel(),
            [&sl_updated_response](const TakeProfitUpdatedEvent ev) {
                sl_updated_response = ev;
            });

    xg::Guid guid;
    {
        TakeProfitMarketOrder tp{
                "TSTUSDT",
                110.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{1},
        };

        guid = tp.guid();
        trgw.push_take_profit_request(tp);
    }

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_TRUE(sl_updated_response->active);
    sl_updated_response.reset();

    // this must not trigger sell stop loss
    price_source_ch.push(std::chrono::milliseconds{20}, 90.);

    // cancel
    {
        trgw.cancel_take_profit_request(guid);
    }

    ASSERT_EQ(trgw.pos_volume().value(), 0);
    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_FALSE(sl_updated_response->active);
    sl_updated_response.reset();

    // exact price, must trigger
    price_source_ch.push(std::chrono::milliseconds{30}, 110.);

    ASSERT_FALSE(sl_updated_response.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), 0);
}

TEST_F(BacktestTradingGatewayTest, StopLossSellTriggerImmediately)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 89.);

    std::optional<StopLossUpdatedEvent> sl_updated_response;
    EventSubcriber sl_updated_sub{el};
    sl_updated_sub.subscribe(
            trgw.stop_loss_update_channel(),
            [&sl_updated_response](const StopLossUpdatedEvent ev) {
                sl_updated_response = ev;
            });

    std::optional<TradeEvent> trade_msg;
    EventSubcriber trade_sub{el};
    trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                trade_msg = ev;
            });

    {
        StopLossMarketOrder sl{
                "TSTUSDT",
                90.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{1},
        };

        trgw.push_stop_loss_request(sl);
    }

    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_FALSE(sl_updated_response->active);
    ASSERT_TRUE(trade_msg.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), -1);
}

TEST_F(BacktestTradingGatewayTest, StopLossBuyTriggerImmediately)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 111.);

    std::optional<StopLossUpdatedEvent> sl_updated_response;

    EventSubcriber sl_updated_sub{el};
    sl_updated_sub.subscribe(
            trgw.stop_loss_update_channel(),
            [&sl_updated_response](const StopLossUpdatedEvent ev) {
                sl_updated_response = ev;
            });

    std::optional<TradeEvent> trade_msg;
    EventSubcriber trade_sub{el};
    trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                trade_msg = ev;
            });

    {
        StopLossMarketOrder sl{
                "TSTUSDT",
                100.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        trgw.push_stop_loss_request(sl);
    }

    ASSERT_TRUE(sl_updated_response.has_value());
    ASSERT_FALSE(sl_updated_response->active);
    ASSERT_TRUE(trade_msg.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), 1);
}

TEST_F(BacktestTradingGatewayTest, TakeProfitSellTriggerImmediately)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 100.);

    std::optional<TakeProfitUpdatedEvent> tp_updated_response;

    EventSubcriber tp_updated_sub{el};
    tp_updated_sub.subscribe(
            trgw.take_profit_update_channel(),
            [&tp_updated_response](const TakeProfitUpdatedEvent ev) {
                tp_updated_response = ev;
            });

    std::optional<TradeEvent> trade_msg;
    EventSubcriber trade_sub{el};
    trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                trade_msg = ev;
            });

    {
        TakeProfitMarketOrder tp{
                "TSTUSDT",
                99.,
                UnsignedVolume::from(1.).value(),
                Side::sell(),
                std::chrono::milliseconds{1},
        };

        trgw.push_take_profit_request(tp);
    }

    ASSERT_TRUE(tp_updated_response.has_value());
    ASSERT_FALSE(tp_updated_response->active);
    ASSERT_TRUE(trade_msg.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), -1);
}

TEST_F(BacktestTradingGatewayTest, TakeProfitBuyTriggerImmediately)
{
    trgw.set_price_source(price_source_ch);
    price_source_ch.push(std::chrono::milliseconds{1}, 99.);

    std::optional<TakeProfitUpdatedEvent> tp_updated_response;
    EventSubcriber tp_updated_sub{el};
    tp_updated_sub.subscribe(
            trgw.take_profit_update_channel(),
            [&tp_updated_response](const TakeProfitUpdatedEvent ev) {
                tp_updated_response = ev;
            });

    std::optional<TradeEvent> trade_msg;
    EventSubcriber trade_sub{el};
    trade_sub.subscribe(
            trgw.trade_channel(),
            [&](const TradeEvent & ev) {
                trade_msg = ev;
            });

    {
        TakeProfitMarketOrder tp{
                "TSTUSDT",
                100.,
                UnsignedVolume::from(1.).value(),
                Side::buy(),
                std::chrono::milliseconds{1},
        };

        trgw.push_take_profit_request(tp);
    }

    ASSERT_TRUE(tp_updated_response.has_value());
    ASSERT_FALSE(tp_updated_response->active);
    ASSERT_TRUE(trade_msg.has_value());

    ASSERT_EQ(trgw.pos_volume().value(), 1);
}

// TODO
// TEST_F(BacktestTradingGatewayTest, TrailingStopRejectIfNoPos)
// TEST_F(BacktestTradingGatewayTest, TrailingStopPullUpAndTrigger)
// TEST_F(BacktestTradingGatewayTest, TrailingStopRemoveOnClosePos)

} // namespace test
