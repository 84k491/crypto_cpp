#pragma once

#include "ConnectionWatcher.h"
#include "EventLoop.h"
#include "EventObjectPublisher.h"
#include "Events.h"
#include "GatewayConfig.h"
#include "Guarded.h"
#include "IMarketDataGateway.h"
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
class ByBitGateway final
    : public IMarketDataGateway
    , public IConnectionSupervisor
    , private IEventInvoker<HistoricalMDRequest, LiveMDRequest, PingCheckEvent>
{
private:
    static constexpr double taker_fee = 0.00055; // 0.055%
    static constexpr std::chrono::seconds ws_ping_interval = std::chrono::seconds(5);

public:
    static constexpr std::chrono::minutes min_historical_interval = std::chrono::minutes{1};
    static auto get_taker_fee() { return taker_fee; }

    ByBitGateway();

    void push_async_request(HistoricalMDRequest && request) override;
    void push_async_request(LiveMDRequest && request) override;

    void unsubscribe_from_live(xg::Guid guid) override;

    EventObjectPublisher<WorkStatus> & status_publisher() override;

    std::vector<Symbol> get_symbols(const std::string & currency);

private:
    void invoke(const std::variant<HistoricalMDRequest, LiveMDRequest, PingCheckEvent> & value) override;
    void handle_request(const HistoricalMDRequest & request);
    void handle_request(const LiveMDRequest & request);
    void handle_request(const PingCheckEvent & event);

    void on_price_received(const nlohmann::json & json);

    std::chrono::milliseconds get_server_time();

    using KlinePackCallback = std::function<void(std::map<std::chrono::milliseconds, OHLC> &&)>;
    bool request_historical_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && cb);

    bool reconnect_ws_client();

    // IConnectionSupervisor
    void on_connection_lost() override;
    void on_connection_verified() override;

private:
    GatewayConfig::MarketData m_config;

    Guarded<std::vector<LiveMDRequest>> m_live_requests;

    std::chrono::milliseconds m_last_server_time = std::chrono::milliseconds{0};

    EventObjectPublisher<WorkStatus> m_status;
    TimeseriesPublisher<OHLC> m_klines_publisher;

    std::unique_ptr<WorkerThreadOnce> m_backtest_thread;
    std::unique_ptr<WorkerThreadLoop> m_live_thread;

    // ((prices by ts) by timerange) by symbol
    std::unordered_map<
            std::string,
            std::unordered_map<
                    Timerange,
                    std::shared_ptr<std::map<
                            std::chrono::milliseconds,
                            OHLC>>>>
            m_ranges_by_symbol;

    RestClient rest_client;
    std::shared_ptr<WebSocketClient> m_ws_client;
    ConnectionWatcher m_connection_watcher;

    EventLoopHolder<HistoricalMDRequest, LiveMDRequest, PingCheckEvent> m_event_loop;
};
