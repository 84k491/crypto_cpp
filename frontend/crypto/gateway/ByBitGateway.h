#pragma once

#include "EventLoop.h"
#include "Events.h"
#include "Guarded.h"
#include "ObjectPublisher.h"
#include "Ohlc.h"
#include "RestClient.h"
#include "Symbol.h"
#include "Timerange.h"
#include "TimeseriesPublisher.h"
#include "WebSocketClient.h"
#include "WorkStatus.h"
#include "WorkerThread.h"

#include <chrono>
#include <functional>
#include <unordered_map>
#include <vector>

class WorkerThreadLoop;
class ByBitGateway final : private IEventInvoker<HistoricalMDRequest, LiveMDRequest>
{
private:
    static constexpr double taker_fee = 0.0002; // 0.02%
    static constexpr std::string_view s_test_ws_linear_endpoint_address = "wss://stream-testnet.bybit.com/v5/public/linear";
    static constexpr std::string_view s_test_rest_endpoint_address = "https://api-testnet.bybit.com";
    static constexpr std::string_view s_endpoint_address = "https://api.bybit.com";

public:
    static constexpr std::chrono::minutes min_interval = std::chrono::minutes{1};
    static auto get_taker_fee() { return taker_fee; }

    ByBitGateway();

    void push_async_request(HistoricalMDRequest && request);
    void push_async_request(LiveMDRequest && request);

    void unsubscribe_from_live(xg::Guid guid);

    ObjectPublisher<WorkStatus> & status_publisher();

    std::vector<Symbol> get_symbols(const std::string & currency);

private:

    void invoke(const std::variant<HistoricalMDRequest, LiveMDRequest> & value) override;
    void handle_request(const HistoricalMDRequest & request);
    void handle_request(const LiveMDRequest & request);

    void on_price_received(const nlohmann::json & json);

    std::chrono::milliseconds get_server_time();

    using KlinePackCallback = std::function<void(std::map<std::chrono::milliseconds, OHLC> &&)>;
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
    WebSocketClient ws_client;
};
