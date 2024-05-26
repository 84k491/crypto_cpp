#pragma once

#include "EventLoop.h" // TODO there must be no dep from loop
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
    virtual Priority priority() const { return Priority::Normal; }
    // TODO rename to base_consumer
    IEventConsumer<T> * event_consumer = nullptr; // TODO use shared_ptr?
};

struct BasicResponseEvent
{
    virtual ~BasicResponseEvent() = default;
    virtual Priority priority() const { return Priority::Normal; }
};

struct MDPriceEvent : public BasicResponseEvent
{
    MDPriceEvent() = default; // no need for guid here, there will be many responses
    std::pair<std::chrono::milliseconds, OHLC> ts_and_price;
};

struct HistoricalMDPackEvent : public BasicResponseEvent
{
    HistoricalMDPackEvent(xg::Guid request_guid);
    std::shared_ptr<const std::map<std::chrono::milliseconds, OHLC>> ts_and_price_pack;

    xg::Guid request_guid;
};

struct HistoricalMDRequestData
{
    std::chrono::milliseconds start;
    std::chrono::milliseconds end;
};
std::ostream & operator<<(std::ostream & os, const HistoricalMDRequestData & data);

struct HistoricalMDRequest : public BasicEvent<HistoricalMDPackEvent>
{
    HistoricalMDRequest(
            IEventConsumer<HistoricalMDPackEvent> & consumer,
            const Symbol & symbol,
            HistoricalMDRequestData data);
    HistoricalMDRequestData data;

    Symbol symbol;
    xg::Guid guid;
};

struct LiveMDRequest : public BasicEvent<MDPriceEvent>
{
    LiveMDRequest(IEventConsumer<MDPriceEvent> & consumer, const Symbol & symbol);

    Symbol symbol;
    xg::Guid guid;
};

struct OrderResponseEvent : public BasicResponseEvent // TODO rename to AckEvent?
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

struct TpslResponseEvent : public OrderResponseEvent
{
    TpslResponseEvent(xg::Guid request_guid, Tpsl tpsl, std::optional<std::string> reason = std::nullopt)
        : OrderResponseEvent(request_guid, std::move(reason))
        , tpsl(tpsl)
    {
    }

    Tpsl tpsl;
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
                      IEventConsumer<OrderResponseEvent> & response_consumer,
                      IEventConsumer<TradeEvent> & trade_consumer)
        : BasicEvent<OrderResponseEvent>(response_consumer)
        , order(std::move(order))
        , trade_ev_consumer(&trade_consumer)
    {
    }
    MarketOrder order;
    IEventConsumer<TradeEvent> * trade_ev_consumer = nullptr;
};

struct TpslUpdatedEvent : public BasicResponseEvent
{
    TpslUpdatedEvent(std::string symbol_name, bool set_up)
        : symbol_name(std::move(symbol_name))
        , set_up(set_up)
    {
    }

    std::string symbol_name;
    bool set_up = false;
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
    xg::Guid guid;
};

using TimerEvent = BasicResponseEvent;
using PingCheckEvent = TimerEvent;

struct LambdaEvent : public BasicResponseEvent
{
    LambdaEvent(std::function<void()> func)
        : func(std::move(func))
    {
    }

    std::function<void()> func;
    xg::Guid guid;
};
