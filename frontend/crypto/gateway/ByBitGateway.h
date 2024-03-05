#pragma once

#include "EventLoop.h"
#include "Guarded.h"
#include "ObjectPublisher.h"
#include "Ohlc.h"
#include "RestClient.h"
#include "Symbol.h"
#include "Timerange.h"
#include "TimeseriesPublisher.h"
#include "WorkStatus.h"
#include "WorkerThread.h"

#include <chrono>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

using HistoricalMDPackEvent = std::map<std::chrono::milliseconds, OHLC>;
using MDPriceEvent = std::pair<std::chrono::milliseconds, OHLC>;
using MDResponseEvent = std::variant<HistoricalMDPackEvent, MDPriceEvent>;

template <class T>
struct BasicEvent
{
    BasicEvent(IEventConsumer<T> & consumer)
        : event_consumer(&consumer)
    {
    }
    virtual ~BasicEvent() = default;
    IEventConsumer<T> * event_consumer = nullptr; // TODO use shared_ptr
};

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

// TODO remove this, use structs above without a consumer
struct MarketDataRequest
{
    struct HistoricalRange
    {
        std::chrono::milliseconds start;
        std::optional<std::chrono::milliseconds> end;
    };

    std::optional<HistoricalRange> historical_range;
    bool go_live = false;
};

class WorkerThreadLoop;
class ByBitGateway final : private IEventInvoker<HistoricalMDRequest, LiveMDRequest>
{
private:
    static constexpr double taker_fee = 0.0002; // 0.02%

public:
    using KlineCallback = std::function<void(std::chrono::milliseconds, const OHLC &)>;

    static constexpr std::chrono::minutes min_interval = std::chrono::minutes{1};
    static auto get_taker_fee() { return taker_fee; }

    ByBitGateway();

    void push_async_request(HistoricalMDRequest && request);
    void push_async_request(LiveMDRequest && request);

    void wait_for_finish();
    void stop_async();

    ObjectPublisher<WorkStatus> & status_publisher();

    std::vector<Symbol> get_symbols(const std::string & currency);

private:
    using KlinePackCallback = std::function<void(std::map<std::chrono::milliseconds, OHLC> &&)>;

    void invoke(const MDRequest & value) override;
    std::chrono::milliseconds get_server_time();
    bool request_historical_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && cb);

private:
    EventLoop<HistoricalMDRequest, LiveMDRequest> m_event_loop;
    Guarded<std::vector<LiveMDRequest>> m_live_requests;

    std::chrono::milliseconds m_last_server_time = std::chrono::milliseconds{0};

    ObjectPublisher<WorkStatus> m_status;
    TimeseriesPublisher<OHLC> m_klines_publisher;

    std::unique_ptr<WorkerThreadOnce> m_backtest_thread;
    std::unique_ptr<WorkerThreadLoop> m_live_thread;

    std::unordered_map<std::string, std::unordered_map<Timerange, std::map<std::chrono::milliseconds, OHLC>>> m_ranges_by_symbol;
    RestClient rest_client;
};
