#pragma once

#include "ObjectPublisher.h"
#include "RestClient.h"
#include "Symbol.h"
#include "Timerange.h"
#include "TimeseriesPublisher.h"
#include "WorkStatus.h"
#include "WorkerThread.h"
#include "ohlc.h"

#include <chrono>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

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
class ByBitGateway
{
    static constexpr double taker_fee = 0.0002; // 0.02%

public:
    using KlineCallback = std::function<void(std::chrono::milliseconds, const OHLC &)>;

    static constexpr std::chrono::minutes min_interval = std::chrono::minutes{1};
    static auto get_taker_fee() { return taker_fee; }

    ByBitGateway() = default;

    std::shared_ptr<TimeseriesSubsription<OHLC>> subscribe_for_klines(KlineCallback && kline_callback);

    std::shared_ptr<TimeseriesSubsription<OHLC>> subscribe_for_klines_and_start(
            const std::string & symbol,
            KlineCallback && kline_callback,
            const MarketDataRequest & md_request);
    void wait_for_finish();
    void stop_async();

    ObjectPublisher<WorkStatus> & status_publisher();

    std::vector<Symbol> get_symbols(const std::string & currency);

private:
    using KlinePackCallback = std::function<void(std::map<std::chrono::milliseconds, OHLC> &&)>;

    std::chrono::milliseconds get_server_time();
    bool request_historical_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && cb);

private:
    std::chrono::milliseconds m_last_server_time = std::chrono::milliseconds{0};

    ObjectPublisher<WorkStatus> m_status;
    TimeseriesPublisher<OHLC> m_klines_publisher;

    std::unique_ptr<WorkerThreadOnce> m_backtest_thread;
    std::unique_ptr<WorkerThreadLoop> m_live_thread;

    std::unordered_map<std::string, std::unordered_map<Timerange, std::map<std::chrono::milliseconds, OHLC>>> m_ranges_by_symbol;
    RestClient rest_client;
};
