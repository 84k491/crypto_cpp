#pragma once

#include "EventChannel.h"
#include "Events.h"

#include <crossguid2/crossguid/guid.hpp>

struct OrderRequestEvent;
struct TpslRequestEvent;

class ITradingGateway
{
public:
    virtual ~ITradingGateway() = default;

    virtual void push_order_request(const OrderRequestEvent & order) = 0;
    virtual void push_tpsl_request(const TpslRequestEvent & tpsl_ev) = 0;
    virtual void push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev) = 0;

    virtual EventChannel<OrderResponseEvent> & order_response_channel() = 0;
    virtual EventChannel<TradeEvent> & trade_channel() = 0;
    virtual EventChannel<TpslResponseEvent> & tpsl_response_channel() = 0;
    virtual EventChannel<TpslUpdatedEvent> & tpsl_updated_channel() = 0;
    virtual EventChannel<TrailingStopLossResponseEvent> & trailing_stop_response_channel() = 0;
    virtual EventChannel<TrailingStopLossUpdatedEvent> & trailing_stop_update_channel() = 0;
};
