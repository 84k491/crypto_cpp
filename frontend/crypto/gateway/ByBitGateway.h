#pragma once

#include "EventLoop.h"
#include "ObjectPublisher.h"
#include "Ohlc.h"
#include "RestClient.h"
#include "Symbol.h"
#include "Timerange.h"
#include "TimeseriesPublisher.h"
#include "WorkStatus.h"
#include "WorkerThread.h"
#include "Guarded.h"

#include <chrono>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

using HistoricalMDPackEvent = std::map<std::chrono::milliseconds, OHLC>;
using MDPriceEvent = std::pair<std::chrono::milliseconds, OHLC>;
using MDResponseEvent = std::variant<HistoricalMDPackEvent, MDPriceEvent>;

struct HistoricalMDRequest
{
    std::chrono::milliseconds start;
    std::chrono::milliseconds end;
    Symbol symbol;
    IEventConsumer<MDResponseEvent> & event_consumer;
};

struct LiveMDRequest
{
    Symbol symbol;
    EventLoop<MDResponseEvent> & event_consumer;
};
using MDRequest = std::variant<HistoricalMDRequest, LiveMDRequest>;

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

// TODO refactor it to be thead safe (push MD requests, put them to queue, parse later)
// it's needed for multitheaded optimizer
class WorkerThreadLoop;
class ByBitGateway final : private IEventInvoker<MDRequest>
{
private:
    static constexpr double taker_fee = 0.0002; // 0.02%

public:
    using KlineCallback = std::function<void(std::chrono::milliseconds, const OHLC &)>;

    static constexpr std::chrono::minutes min_interval = std::chrono::minutes{1};
    static auto get_taker_fee() { return taker_fee; }

    ByBitGateway();

    void push_async_request(MDRequest && request);

    // std::shared_ptr<TimeseriesSubsription<OHLC>> subscribe_for_klines(KlineCallback && kline_callback);
    //
    // std::shared_ptr<TimeseriesSubsription<OHLC>> subscribe_for_klines_and_start(
    //         const std::string & symbol,
    //         KlineCallback && kline_callback,
    //         const MarketDataRequest & md_request);
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
    EventLoop<MDRequest> m_event_loop;
    Guarded<std::vector<LiveMDRequest>> m_live_requests;

    std::chrono::milliseconds m_last_server_time = std::chrono::milliseconds{0};

    ObjectPublisher<WorkStatus> m_status;
    TimeseriesPublisher<OHLC> m_klines_publisher;

    std::unique_ptr<WorkerThreadOnce> m_backtest_thread;
    std::unique_ptr<WorkerThreadLoop> m_live_thread;

    std::unordered_map<std::string, std::unordered_map<Timerange, std::map<std::chrono::milliseconds, OHLC>>> m_ranges_by_symbol;
    RestClient rest_client;
};
