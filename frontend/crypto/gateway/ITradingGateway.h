#pragma once

#include "EventPublisher.h"
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

    virtual EventPublisher<OrderResponseEvent> & order_response_publisher() = 0;
    virtual EventPublisher<TradeEvent> & trade_publisher() = 0;
    virtual EventPublisher<TpslResponseEvent> & tpsl_response_publisher() = 0;
    virtual EventPublisher<TpslUpdatedEvent> & tpsl_updated_publisher() = 0;
    virtual EventPublisher<TrailingStopLossResponseEvent> & trailing_stop_response_publisher() = 0;
    virtual EventPublisher<TrailingStopLossUpdatedEvent> & trailing_stop_update_publisher() = 0;
};
