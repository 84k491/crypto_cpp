#pragma once

#include "ConnectionWatcher.h"
#include "EventChannel.h"
#include "EventLoopSubscriber.h"
#include "EventObjectChannel.h"
#include "Events.h"
#include "GatewayConfig.h"
#include "Guarded.h"
#include "IMarketDataGateway.h"
#include "Ohlc.h"
#include "RestClient.h"
#include "Symbol.h"
#include "Timerange.h"
#include "TimeseriesChannel.h"
#include "WebSocketClient.h"
#include "WorkStatus.h"
#include "WorkerThread.h"

#include <chrono>
#include <functional>
#include <vector>

class WorkerThreadLoop;
class ByBitMarketDataGateway final
    : public IMarketDataGateway
    , public IConnectionSupervisor
{
private:
    static constexpr double taker_fee = 0.00055; // 0.055%
    static constexpr std::chrono::seconds ws_ping_interval = std::chrono::seconds(5);

public:
    static constexpr std::chrono::minutes min_historical_interval = std::chrono::minutes{1};
    static auto get_taker_fee() { return taker_fee; }

    ByBitMarketDataGateway(bool start = false);

    void push_async_request(HistoricalMDRequest && request) override;
    void push_async_request(LiveMDRequest && request) override;

    EventChannel<HistoricalMDGeneratorEvent> & historical_prices_channel() override;
    EventChannel<MDPriceEvent> & live_prices_channel() override;

    void unsubscribe_from_live(xg::Guid guid) override;

    EventObjectChannel<WorkStatus> & status_channel() override;

    std::vector<Symbol> get_symbols(const std::string & currency);

private:
    void register_subs();
    void handle_event(const HistoricalMDRequest & request);
    void handle_event(const LiveMDRequest & request);
    void handle_event(const PingCheckEvent & event);

    void on_price_received(const nlohmann::json & json);

    std::chrono::milliseconds get_server_time();

    using KlinePackCallback = std::function<void(std::map<std::chrono::milliseconds, OHLC> &&)>;
    bool request_historical_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && cb);

    bool reconnect_ws_client();

    // IConnectionSupervisor
    void on_connection_lost() override;
    void on_connection_verified() override;

private:
    EventLoop m_event_loop;

    GatewayConfig::MarketData m_config;

    Guarded<std::vector<LiveMDRequest>> m_live_requests; // TODO remove?

    std::chrono::milliseconds m_last_server_time = std::chrono::milliseconds{0};

    EventObjectChannel<WorkStatus> m_status;
    TimeseriesChannel<OHLC> m_klines_channel;

    std::unique_ptr<WorkerThreadOnce> m_backtest_thread;
    std::unique_ptr<WorkerThreadLoop> m_live_thread;

    RestClient rest_client;
    std::shared_ptr<WebSocketClient> m_ws_client;
    ConnectionWatcher m_connection_watcher;

    EventChannel<HistoricalMDRequest> m_historical_md_req_channel;
    EventChannel<LiveMDRequest> m_live_md_req_channel;
    EventChannel<PingCheckEvent> m_ping_event_channel;

    EventChannel<HistoricalMDGeneratorEvent> m_historical_prices_channel;
    EventChannel<MDPriceEvent> m_live_prices_channel;

    EventSubcriber m_sub;
};
