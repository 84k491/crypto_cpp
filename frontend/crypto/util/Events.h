#pragma once

#include "ThreadSafePriorityQueue.h"
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

struct HistoricalMDRequest : public OneWayEvent
{
    HistoricalMDRequest(
            const Symbol & symbol,
            HistoricalMDRequestData data);
    HistoricalMDRequestData data;

    Symbol symbol;
    xg::Guid guid;
};

struct LiveMDRequest : public OneWayEvent
{
    LiveMDRequest(const Symbol & symbol);

    Symbol symbol;
    xg::Guid guid;
};

struct OrderResponseEvent : public OneWayEvent
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
    bool retry = false;
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

struct OrderRequestEvent : public OneWayEvent
{
    OrderRequestEvent(MarketOrder order)
        : order(std::move(order))
    {
    }
    MarketOrder order;
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

struct TpslRequestEvent : public OneWayEvent
{
    TpslRequestEvent(
            Symbol symbol,
            Tpsl tpsl)
        : symbol(std::move(symbol))
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
        : OrderResponseEvent(trailing_stop_loss.symbol_name(), request_guid, std::move(reject_reason))
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

struct TrailingStopLossRequestEvent : public OneWayEvent
{
    TrailingStopLossRequestEvent(
            Symbol symbol,
            TrailingStopLoss trailing_stop_loss)
        : symbol(std::move(symbol))
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
    using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long, std::ratio<1, 1000000000>>>;

    LogEvent(LogLevel level, std::string && log)
        : log_str(std::move(log))
        , ts(std::chrono::system_clock::now())
        , level(level)
    {
    }

    std::string log_str;
    TimePoint ts;
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
