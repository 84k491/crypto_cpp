#pragma once

#include "EventChannel.h"
#include "Events.h"

#include <crossguid/guid.hpp>

struct OrderRequestEvent;
struct TpslRequestEvent;
class TakeProfitMarketOrder;
class StopLossMarketOrder;

class ITradingGateway
{
public:
    virtual ~ITradingGateway() = default;

    // TODO remove events, use objects
    virtual void push_order_request(const OrderRequestEvent & order) = 0;
    virtual void push_tpsl_request(const TpslRequestEvent & tpsl_ev) = 0;
    virtual void push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev) = 0;
    virtual void push_take_profit_request(const TakeProfitMarketOrder & tp) = 0;
    virtual void push_stop_loss_request(const StopLossMarketOrder & sl) = 0;
    virtual void cancel_take_profit_request(xg::Guid guid) = 0;
    virtual void cancel_stop_loss_request(xg::Guid guid) = 0;

    virtual EventChannel<OrderResponseEvent> & order_response_channel() = 0;
    virtual EventChannel<TradeEvent> & trade_channel() = 0;
    virtual EventChannel<TpslUpdatedEvent> & tpsl_updated_channel() = 0;
    virtual EventChannel<TrailingStopLossUpdatedEvent> & trailing_stop_update_channel() = 0;
    virtual EventChannel<TakeProfitUpdatedEvent> & take_profit_update_channel() = 0;
    virtual EventChannel<StopLossUpdatedEvent> & stop_loss_update_channel() = 0;
};
