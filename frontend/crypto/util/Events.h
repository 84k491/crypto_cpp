#pragma once

#include "EventLoop.h"
#include "MarketOrder.h"
#include "Ohlc.h"
#include "Symbol.h"
#include "Tpsl.h"
#include "Trade.h"

#include <crossguid2/crossguid/guid.hpp>
#include <map>
#include <utility>

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
    MDPriceEvent() = default; // no need for guid here, there will be many responses
    std::pair<std::chrono::milliseconds, OHLC> ts_and_price;
};

struct HistoricalMDPackEvent : public BasicResponseEvent
{
    HistoricalMDPackEvent(xg::Guid request_guid);
    std::map<std::chrono::milliseconds, OHLC> ts_and_price_pack;

    xg::Guid request_guid;
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

    xg::Guid guid;
};

struct LiveMDRequest : public BasicEvent<MDPriceEvent>
{
    LiveMDRequest(IEventConsumer<MDPriceEvent> & _consumer, const Symbol & _symbol);
    Symbol symbol;

    xg::Guid guid;
};
using MDRequest = std::variant<HistoricalMDRequest, LiveMDRequest>;

struct OrderResponseEvent : public BasicResponseEvent
{
    OrderResponseEvent(
            xg::Guid request_guid,
            std::optional<std::string> reject_reason = std::nullopt)
        : request_guid(request_guid)
        , reject_reason(std::move(reject_reason))
    {
    }

    xg::Guid request_guid;
    std::optional<std::string> reject_reason;
};

struct TradeEvent : public BasicResponseEvent
{
    TradeEvent(Trade trade)
        : trade(std::move(trade))
    {
    }
    Trade trade;
};

struct OrderRequestEvent : public BasicEvent<OrderResponseEvent>
{
    OrderRequestEvent(MarketOrder order,
                      IEventConsumer<OrderResponseEvent> & accept_consumer,
                      IEventConsumer<TradeEvent> & trade_consumer)
        : BasicEvent<OrderResponseEvent>(accept_consumer)
        , order(std::move(order))
        , trade_ev_consumer(&trade_consumer)
        , guid(xg::newGuid())
    {
    }
    MarketOrder order;
    IEventConsumer<TradeEvent> * trade_ev_consumer = nullptr;

    xg::Guid guid;
};

struct TpslResponseEvent : public BasicResponseEvent
{
    TpslResponseEvent(xg::Guid request_guid, Tpsl tpsl, bool accepted)
        : tpsl(tpsl)
        , accepted(accepted)
        , request_guid(request_guid)
    {
    }

    Tpsl tpsl;
    bool accepted = false;
    xg::Guid request_guid;
};

struct TpslRequestEvent : public BasicEvent<TpslResponseEvent>
{
    TpslRequestEvent(Symbol symbol, Tpsl tpsl, IEventConsumer<TpslResponseEvent> & ack_consumer)
        : BasicEvent<TpslResponseEvent>(ack_consumer)
        , symbol(std::move(symbol))
        , tpsl(tpsl)
        , guid(xg::newGuid())
    {
    }

    Symbol symbol; // TODO move it to Tpsl
    Tpsl tpsl;
    // IEventConsumer<TpslRejectedEvent> * reject_ev_consumer = nullptr;
    xg::Guid guid;
};
