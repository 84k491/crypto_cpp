#pragma once

#include "LogLevel.h"
#include "MarketOrder.h"
#include "Ohlc.h"
#include "Symbol.h"
#include "ThreadSafePriorityQueue.h"
#include "Tpsl.h"
#include "Trade.h"
#include "TrailingStopLoss.h"

#include <crossguid/guid.hpp>
#include <map>
#include <utility>

struct OneWayEvent
{
    virtual ~OneWayEvent() = default;
    virtual Priority priority() const { return Priority::Normal; }
};

struct MDPriceEvent : public OneWayEvent
{
    MDPriceEvent(CsvPublicTrade _public_trade)
        : public_trade(_public_trade)
    {
    }
    Priority priority() const override { return Priority::Low; }
    CsvPublicTrade public_trade;
};

struct HistoricalMDPriceEvent : MDPriceEvent
{
    HistoricalMDPriceEvent(CsvPublicTrade _ts_and_price, bool lowmem)
        : MDPriceEvent(_ts_and_price)
        , lowmem(lowmem)
    {
    }

    bool lowmem; // TODO make it possible to use const here
};

class HistoricalMDGeneratorEvent : public OneWayEvent
{
    using PricePackPtr = std::shared_ptr<const std::vector<CsvPublicTrade>>;

public:
    HistoricalMDGeneratorEvent(xg::Guid guid, PricePackPtr ts_and_price_pack)
        : m_request_guid(guid)
        , m_pack(std::move(ts_and_price_pack))
    {
    }
    HistoricalMDGeneratorEvent(const HistoricalMDGeneratorEvent & other)
        : m_request_guid(other.m_request_guid)
        , m_pack(other.m_pack)
        , m_iter(other.m_iter)
    {
    }
    HistoricalMDGeneratorEvent & operator=(const HistoricalMDGeneratorEvent & other)
    {
        if (this == &other) {
            return *this;
        }
        m_request_guid = other.m_request_guid;
        m_pack = other.m_pack;
        m_iter = other.m_iter;
        return *this;
    }
    HistoricalMDGeneratorEvent(HistoricalMDGeneratorEvent && other) noexcept
        : m_request_guid(other.m_request_guid)
        , m_pack(std::move(other.m_pack))
        , m_iter(other.m_iter)
    {
    }

    HistoricalMDGeneratorEvent & operator=(HistoricalMDGeneratorEvent && other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        m_request_guid = other.m_request_guid;
        m_pack = std::move(other.m_pack);
        m_iter = other.m_iter;
        return *this;
    }

    std::optional<HistoricalMDPriceEvent> get_next()
    {
        const auto iter = m_iter++;
        if (iter >= m_pack->size()) {
            return std::nullopt;
        }

        return HistoricalMDPriceEvent{(*m_pack)[iter], false};
    }

    bool has_data() const { return m_pack && !m_pack->empty(); } // TODO useless
    auto request_guid() const { return m_request_guid; }

private:
    xg::Guid m_request_guid;

    PricePackPtr m_pack;
    size_t m_iter = 0;
};

class SequentialMarketDataReader;

class HistoricalMDGeneratorLowMemEvent : public OneWayEvent
{
    using PricePackPtr = std::shared_ptr<const std::vector<std::pair<std::chrono::milliseconds, double>>>;

public:
    HistoricalMDGeneratorLowMemEvent(xg::Guid guid, std::shared_ptr<SequentialMarketDataReader> reader)
        : m_request_guid(guid)
        , m_reader(std::move(reader))
    {
    }

    HistoricalMDGeneratorLowMemEvent(const HistoricalMDGeneratorLowMemEvent & other)
        : m_request_guid(other.m_request_guid)
        , m_reader(other.m_reader)
    {
    }

    HistoricalMDGeneratorLowMemEvent & operator=(const HistoricalMDGeneratorLowMemEvent & other)
    {
        if (this == &other) {
            return *this;
        }
        m_request_guid = other.m_request_guid;
        m_reader = other.m_reader;
        return *this;
    }

    HistoricalMDGeneratorLowMemEvent(HistoricalMDGeneratorLowMemEvent && other) noexcept
        : m_request_guid(other.m_request_guid)
        , m_reader(std::move(other.m_reader))
    {
    }

    HistoricalMDGeneratorLowMemEvent & operator=(HistoricalMDGeneratorLowMemEvent && other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        m_request_guid = other.m_request_guid;
        m_reader = std::move(other.m_reader);
        return *this;
    }

    std::optional<HistoricalMDPriceEvent> get_next();

    auto request_guid() const { return m_request_guid; }

private:
    xg::Guid m_request_guid;

    std::shared_ptr<SequentialMarketDataReader> m_reader;
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
    bool lowmem = true;
    xg::Guid guid;
};

struct HistoricalMDLowMemRequest : public OneWayEvent
{
    HistoricalMDLowMemRequest(
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

struct BarrierEvent : public OneWayEvent
{
    BarrierEvent()
        : guid(xg::newGuid())
    {
    }

    Priority priority() const override
    {
        return Priority::Barrier;
    }

    xg::Guid guid;
};

struct StrategyStopRequest : public OneWayEvent
{
    Priority priority() const override
    {
        return Priority::Low;
    }
};

#define STRATEGY_EVENTS LambdaEvent,                      \
                        BarrierEvent,                     \
                        HistoricalMDGeneratorEvent,       \
                        HistoricalMDGeneratorLowMemEvent, \
                        HistoricalMDPriceEvent,           \
                        MDPriceEvent,                     \
                        OrderResponseEvent,               \
                        TradeEvent,                       \
                        TpslResponseEvent,                \
                        TpslUpdatedEvent,                 \
                        TrailingStopLossResponseEvent,    \
                        TrailingStopLossUpdatedEvent,     \
                        StrategyStopRequest
