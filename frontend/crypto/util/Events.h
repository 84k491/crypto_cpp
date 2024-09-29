#pragma once

#include "EventLoop.h" // TODO there must be no dep from loop
#include "LogLevel.h"
#include "MarketOrder.h"
#include "Ohlc.h"
#include "Symbol.h"
#include "Tpsl.h"
#include "Trade.h"
#include "TrailingStopLoss.h"

#include <crossguid2/crossguid/guid.hpp>
#include <map>
#include <utility>

template <class T>
struct EventWithResponse
{
    EventWithResponse(const std::shared_ptr<IEventConsumer<T>> & consumer)
        : response_consumer(consumer)
    {
    }
    virtual ~EventWithResponse() = default;
    virtual Priority priority() const { return Priority::Normal; }
    std::weak_ptr<IEventConsumer<T>> response_consumer = nullptr;
};

struct OneWayEvent
{
    virtual ~OneWayEvent() = default;
    virtual Priority priority() const { return Priority::Normal; }
};

struct MDPriceEvent : public OneWayEvent
{
    MDPriceEvent() = default; // no need for guid here, there will be many responses
    Priority priority() const override { return Priority::Low; }
    std::pair<std::chrono::milliseconds, OHLC> ts_and_price;
};

struct HistoricalMDPackEvent : public OneWayEvent
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

struct HistoricalMDRequest : public EventWithResponse<HistoricalMDPackEvent>
{
    HistoricalMDRequest(
            const std::shared_ptr<IEventConsumer<HistoricalMDPackEvent>> & consumer,
            const Symbol & symbol,
            HistoricalMDRequestData data);
    HistoricalMDRequestData data;

    Symbol symbol;
    xg::Guid guid;
};

struct LiveMDRequest : public EventWithResponse<MDPriceEvent>
{
    LiveMDRequest(
            const std::shared_ptr<IEventConsumer<MDPriceEvent>> & consumer,
            const Symbol & symbol);

    Symbol symbol;
    xg::Guid guid;
};

struct OrderResponseEvent : public OneWayEvent // TODO rename to AckEvent?
{
    OrderResponseEvent(
            std::string symbol_name,
            xg::Guid request_guid,
            std::optional<std::string> reject_reason = std::nullopt)
        : symbol_name(std::move(symbol_name))
        , request_guid(request_guid)
        , reject_reason(std::move(reject_reason))
    {
    }

    std::string symbol_name;
    xg::Guid request_guid;
    std::optional<std::string> reject_reason;
};

struct TpslResponseEvent : public OrderResponseEvent
{
    TpslResponseEvent(std::string symbol_name, xg::Guid request_guid, Tpsl tpsl, std::optional<std::string> reject_reason = std::nullopt)
        : OrderResponseEvent(std::move(symbol_name), request_guid, std::move(reject_reason))
        , tpsl(tpsl)
    {
    }

    Tpsl tpsl;
};

struct TradeEvent : public OneWayEvent
{
    TradeEvent(Trade trade)
        : trade(std::move(trade))
    {
    }
    Trade trade;
};

struct OrderRequestEvent : public EventWithResponse<OrderResponseEvent>
{
    OrderRequestEvent(MarketOrder order,
                      const std::shared_ptr<IEventConsumer<OrderResponseEvent>> & response_consumer,
                      const std::shared_ptr<IEventConsumer<TradeEvent>> & trade_consumer)
        : EventWithResponse<OrderResponseEvent>(response_consumer)
        , order(std::move(order))
        , trade_ev_consumer(trade_consumer)
    {
    }
    MarketOrder order;
    std::weak_ptr<IEventConsumer<TradeEvent>> trade_ev_consumer;
};

struct TpslUpdatedEvent : public OneWayEvent
{
    TpslUpdatedEvent(std::string symbol_name, bool set_up)
        : symbol_name(std::move(symbol_name))
        , set_up(set_up)
    {
    }

    std::string symbol_name;
    bool set_up = false;
};

struct TpslRequestEvent : public EventWithResponse<TpslResponseEvent>
{
    TpslRequestEvent(
            Symbol symbol,
            Tpsl tpsl,
            const std::shared_ptr<IEventConsumer<TpslResponseEvent>> & ack_consumer)
        : EventWithResponse<TpslResponseEvent>(ack_consumer)
        , symbol(std::move(symbol))
        , tpsl(tpsl)
        , guid(xg::newGuid())
    {
    }

    Symbol symbol; // TODO move it to Tpsl
    Tpsl tpsl;
    xg::Guid guid;
};

struct TrailingStopLossResponseEvent : public OrderResponseEvent
{
    TrailingStopLossResponseEvent(xg::Guid request_guid, TrailingStopLoss trailing_stop_loss, std::optional<std::string> reject_reason = std::nullopt)
        : OrderResponseEvent(trailing_stop_loss.symbol().symbol_name, request_guid, std::move(reject_reason))
        , trailing_stop_loss(std::move(trailing_stop_loss))
    {
    }

    TrailingStopLoss trailing_stop_loss;
};

struct TrailingStopLossUpdatedEvent : public OneWayEvent
{
    TrailingStopLossUpdatedEvent(std::string symbol_name, std::optional<StopLoss> stop_loss, std::chrono::milliseconds timestamp)
        : symbol_name(std::move(symbol_name))
        , stop_loss(std::move(stop_loss))
        , timestamp(timestamp)
    {
    }

    std::string symbol_name;
    std::optional<StopLoss> stop_loss;
    std::chrono::milliseconds timestamp;
};

struct TrailingStopLossRequestEvent : public EventWithResponse<TrailingStopLossResponseEvent>
{
    TrailingStopLossRequestEvent(
            Symbol symbol,
            TrailingStopLoss trailing_stop_loss,
            const std::shared_ptr<IEventConsumer<TrailingStopLossResponseEvent>> & ack_consumer)
        : EventWithResponse<TrailingStopLossResponseEvent>(ack_consumer)
        , symbol(std::move(symbol))
        , trailing_stop_loss(std::move(trailing_stop_loss))
        , guid(xg::newGuid())
    {
    }

    Symbol symbol;
    TrailingStopLoss trailing_stop_loss;
    xg::Guid guid;
};

using TimerEvent = OneWayEvent;
using PingCheckEvent = TimerEvent;

struct LambdaEvent : public OneWayEvent
{
    LambdaEvent(std::function<void()> func)
        : func(std::move(func))
    {
    }

    std::function<void()> func;
    xg::Guid guid;
};

struct LogEvent : public OneWayEvent
{
    LogEvent(LogLevel level, std::string && log)
        : log_str(std::move(log))
        , level(level)
    {
    }

    std::string log_str;
    LogLevel level;
};

struct StrategyStopRequest : public OneWayEvent
{
    Priority priority() const override
    {
        return Priority::Low;
    }
};

#define STRATEGY_EVENTS HistoricalMDPackEvent,         \
                        MDPriceEvent,                  \
                        OrderResponseEvent,            \
                        TradeEvent,                    \
                        TpslResponseEvent,             \
                        TpslUpdatedEvent,              \
                        TrailingStopLossResponseEvent, \
                        TrailingStopLossUpdatedEvent,  \
                        StrategyStopRequest,           \
                        LambdaEvent
