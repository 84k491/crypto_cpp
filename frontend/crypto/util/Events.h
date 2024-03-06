#pragma once

#include "EventLoop.h"
#include "MarketOrder.h"
#include "Ohlc.h"
#include "Symbol.h"
#include "Trade.h"

#include <map>

template <class T>
struct BasicEvent
{
    BasicEvent(IEventConsumer<T> & consumer)
        : event_consumer(&consumer)
    {
    }
    virtual ~BasicEvent() = default;
    // TODO rename to base_consumer
    IEventConsumer<T> * event_consumer = nullptr; // TODO use shared_ptr
};

struct BasicResponseEvent
{
    virtual ~BasicResponseEvent() = default;
};

struct MDPriceEvent : public BasicResponseEvent
{
    std::pair<std::chrono::milliseconds, OHLC> ts_and_price;
};

struct HistoricalMDPackEvent : public BasicResponseEvent
{
    std::map<std::chrono::milliseconds, OHLC> ts_and_price_pack;
};
using MDResponseEvent = std::variant<HistoricalMDPackEvent, MDPriceEvent>;

struct HistoricalMDRequest : public BasicEvent<HistoricalMDPackEvent>
{
    HistoricalMDRequest(
            IEventConsumer<HistoricalMDPackEvent> & _consumer,
            const Symbol & _symbol,
            std::chrono::milliseconds _start,
            std::chrono::milliseconds _end);
    Symbol symbol;
    std::chrono::milliseconds start;
    std::chrono::milliseconds end;
};

struct LiveMDRequest : public BasicEvent<MDPriceEvent>
{
    LiveMDRequest(IEventConsumer<MDPriceEvent> & _consumer, const Symbol & _symbol);
    Symbol symbol;
};
using MDRequest = std::variant<HistoricalMDRequest, LiveMDRequest>;

struct OrderAcceptedEvent : public BasicResponseEvent
{
    OrderAcceptedEvent(MarketOrder order)
        : order(std::move(order))
    {
    }

    MarketOrder order;
};

struct TradeEvent : public BasicResponseEvent
{
    TradeEvent(Trade trade)
        : trade(std::move(trade))
    {
    }
    Trade trade;
};

struct OrderRejectedEvent : public BasicResponseEvent
{
    OrderRejectedEvent(
            bool internal_reject,
            std::string reason,
            MarketOrder order)
        : internal_reject(internal_reject)
        , reason(std::move(reason))
        , order(std::move(order))
    {
    }

    bool internal_reject = true;
    std::string reason = "Unknown";
    MarketOrder order;
};

struct OrderRequestEvent : public BasicEvent<OrderAcceptedEvent>
{
    MarketOrder order;
    IEventConsumer<TradeEvent> * trade_ev_consumer = nullptr;
    IEventConsumer<OrderRejectedEvent> * reject_ev_consumer = nullptr;
};
