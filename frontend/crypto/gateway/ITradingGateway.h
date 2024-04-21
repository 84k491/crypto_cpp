#pragma once

#include "Events.h"
#include "Symbol.h"

#include <crossguid2/crossguid/guid.hpp>

struct OrderRequestEvent;
struct TpslRequestEvent;

class ITradingGateway
{
public:
    virtual ~ITradingGateway() = default;

    virtual void push_order_request(const OrderRequestEvent & order) = 0;
    virtual void push_tpsl_request(const TpslRequestEvent & tpsl_ev) = 0;

    virtual void register_trade_consumer(xg::Guid guid, const Symbol & symbol, IEventConsumer<TradeEvent> & consumer) = 0;
    virtual void unregister_trade_consumer(xg::Guid guid) = 0;
};
