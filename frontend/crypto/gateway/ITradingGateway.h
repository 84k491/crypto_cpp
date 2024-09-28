#pragma once

#include "Events.h"
#include "Symbol.h"

#include <crossguid2/crossguid/guid.hpp>

struct OrderRequestEvent;
struct TpslRequestEvent;

struct TradingGatewayConsumers
{
    IEventConsumer<TradeEvent> & trade_consumer;
    IEventConsumer<OrderResponseEvent> & order_ack_consumer;
    IEventConsumer<TpslResponseEvent> & tpsl_response_consumer;
    IEventConsumer<TpslUpdatedEvent> & tpsl_update_consumer;
    IEventConsumer<TrailingStopLossUpdatedEvent> & trailing_stop_update_consumer;
};

class ITradingGateway
{
public:
    virtual ~ITradingGateway() = default;

    virtual void push_order_request(const OrderRequestEvent & order) = 0;
    virtual void push_tpsl_request(const TpslRequestEvent & tpsl_ev) = 0;
    virtual void push_trailing_stop_request(const TrailingStopLossRequestEvent & trailing_stop_ev) = 0;

    virtual void register_consumers(xg::Guid guid, const Symbol & symbol, TradingGatewayConsumers consumers) = 0;
    virtual void unregister_consumers(xg::Guid guid) = 0;
};
