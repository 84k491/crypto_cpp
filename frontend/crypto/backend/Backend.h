#pragma once

#include "BackendCommands.h"
#include "ByBitMarketDataGateway.h"
#include "ByBitTradingGateway.h"
#include "ConfigGenerator.h"
#include "EventChannel.h"
#include "EventLoop.h"
#include "EventObjectChannel.h"
#include "Launchpad.h"
#include "RemoteChannelServer.h"
#include "StrategyInstance.h"
#include "Symbol.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct StartOptimizerCommand
{
    std::string symbol_name;
    std::string strategy_name;
    std::chrono::milliseconds start;
    std::chrono::milliseconds end;
    OptimizerInputs optimizer_inputs;
    size_t thread_count = 1;
};

struct OptimizerProgress
{
    int passed_checks = 0;
    int total_checks = 0;
};

struct CurrentRun
{
    std::weak_ptr<StrategyInstance> strategy_instance;
    bool is_backtest = false;
};

class Backend
{
public:
    Backend();
    ~Backend();

    EventChannel<StartBacktestCommand> & start_backtest_channel() { return m_start_backtest_channel; }
    EventChannel<StartLiveRunCommand> & start_live_run_channel() { return m_start_live_run_channel; }
    EventChannel<StopCommand> & stop_channel() { return m_stop_channel; }
    EventChannel<PlaybackSettingsEvent> & playback_settings_channel() { return m_playback_settings_channel; }
    EventChannel<PlaybackStepEvent> & playback_step_channel() { return m_playback_step_channel; }
    EventChannel<StartOptimizerCommand> & start_optimizer_channel() { return m_start_optimizer_channel; }

    EventChannel<CommandReply> & command_reply_channel() { return m_command_reply_channel; }
    EventObjectChannel<std::vector<std::string>> & strategy_names_channel() { return m_strategy_names_channel; }
    EventObjectChannel<std::vector<Symbol>> & symbols_channel() { return m_symbols_channel; }
    EventObjectChannel<CurrentRun> & current_run_channel() { return m_current_run_channel; }
    EventChannel<OptimizerProgress> & optimizer_progress_channel() { return m_optimizer_progress_channel; }
    EventChannel<JsonStrategyConfig> & optimized_config_channel() { return m_optimized_config_channel; }

private:
    void subscribe_for_commands();
    void register_remote_channels();

    void start_backtest(const StartBacktestCommand & command);
    void start_live_run(const StartLiveRunCommand & command);
    void stop_run(const StopCommand & command);
    void push_playback_settings(const PlaybackSettingsEvent & settings);
    void push_playback_step(const PlaybackStepEvent & step);
    void start_optimizer(const StartOptimizerCommand & command);

    void start_run(
            std::shared_ptr<Launchpad> launchpad,
            bool is_backtest,
            std::optional<std::chrono::milliseconds> channel_capacity,
            xg::Guid command_guid);
    void drop_current_run();

    void register_run_remote_channels();
    void register_own_series_remote_channels(const ChartSeriesList & list);
    void unregister_run_remote_channels();

    template <class ObjectT>
    void register_run_object_channel(const std::string & channel_name, EventObjectChannel<ObjectT> & channel);
    template <class ObjectT>
    void register_run_timeseries_channel(const std::string & channel_name, EventTimeseriesChannel<ObjectT> & channel);

    std::optional<Symbol> find_symbol(const std::string & symbol_name);

    void reply_ok(xg::Guid command_guid);
    void reply_error(xg::Guid command_guid, const std::string & error);

private:
    ByBitMarketDataGateway m_gateway;
    std::unique_ptr<ByBitTradingGateway> m_trading_gateway;

    EventChannel<StartBacktestCommand> m_start_backtest_channel;
    EventChannel<StartLiveRunCommand> m_start_live_run_channel;
    EventChannel<StopCommand> m_stop_channel;
    EventChannel<PlaybackSettingsEvent> m_playback_settings_channel;
    EventChannel<PlaybackStepEvent> m_playback_step_channel;
    EventChannel<StartOptimizerCommand> m_start_optimizer_channel;

    EventChannel<CommandReply> m_command_reply_channel;
    EventObjectChannel<std::vector<std::string>> m_strategy_names_channel;
    EventObjectChannel<std::vector<Symbol>> m_symbols_channel;
    EventObjectChannel<CurrentRun> m_current_run_channel;
    EventChannel<OptimizerProgress> m_optimizer_progress_channel;
    EventChannel<JsonStrategyConfig> m_optimized_config_channel;

    RemoteChannelServer m_remote_channel_server;

    std::shared_ptr<Launchpad> m_launchpad;
    bool m_run_is_active = false;
    std::set<std::string> m_run_remote_channel_names;

    EventLoop m_event_loop;
    std::unique_ptr<EventSubcriber> m_run_sub;
    std::unique_ptr<EventSubcriber> m_sub;
};
