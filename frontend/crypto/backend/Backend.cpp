#include "Backend.h"

#include "CandleJsonCodec.h"
#include "ChartSeriesListJsonCodec.h"
#include "CommandReplyJsonCodec.h"
#include "ContainerJsonCodec.h"
#include "Logger.h"
#include "Macros.h"
#include "MarketStateRenderObjectJsonCodec.h"
#include "Optimizer.h"
#include "PositionResultJsonCodec.h"
#include "PrimitiveJsonCodec.h"
#include "ProfitPriceLevelsJsonCodec.h"
#include "RemoteChannelNames.h"
#include "RemoteEventChannelServer.h"
#include "RemoteEventObjectChannelServer.h"
#include "RemoteEventTimeseriesChannelServer.h"
#include "StartBacktestCommandJsonCodec.h"
#include "StopCommandJsonCodec.h"
#include "StopLossJsonCodec.h"
#include "StrategyFactory.h"
#include "StrategyResultJsonCodec.h"
#include "SymbolJsonCodec.h"
#include "Timerange.h"
#include "TpslPricesJsonCodec.h"
#include "TradeJsonCodec.h"
#include "WorkStatusJsonCodec.h"

#include <future>
#include <thread>

Backend::Backend()
    : m_remote_channel_server(remote_channel_port)
    , m_run_sub(std::make_unique<EventSubcriber>(m_event_loop))
    , m_sub(std::make_unique<EventSubcriber>(m_event_loop))
{
    m_strategy_names_channel.push(StrategyFactory::i().get_all_strategy_names());
    m_symbols_channel.push(m_gateway.get_symbols("USDT"));

    subscribe_for_commands();
    register_remote_channels();
}

Backend::~Backend()
{
    std::promise<void> teardown_promise;
    m_event_loop.push(LambdaEvent{
            xg::newGuid(),
            [&] {
                m_sub.reset();
                drop_current_run();
                teardown_promise.set_value();
            },
            Priority::Normal});
    teardown_promise.get_future().wait();
}

void Backend::subscribe_for_commands()
{
    m_sub->subscribe(m_start_backtest_channel, [this](const StartBacktestCommand & command) { start_backtest(command); });
    m_sub->subscribe(m_start_live_run_channel, [this](const StartLiveRunCommand & command) { start_live_run(command); });
    m_sub->subscribe(m_stop_channel, [this](const StopCommand & command) { stop_run(command); });
    m_sub->subscribe(m_playback_settings_channel, [this](const PlaybackSettingsEvent & settings) { push_playback_settings(settings); });
    m_sub->subscribe(m_playback_step_channel, [this](const PlaybackStepEvent & step) { push_playback_step(step); });
    m_sub->subscribe(m_start_optimizer_channel, [this](const StartOptimizerCommand & command) { start_optimizer(command); });
}

void Backend::register_remote_channels()
{
    auto & event_loop = m_remote_channel_server.event_loop();

    m_remote_channel_server.register_channel(
            RemoteChannelNames::strategy_names,
            std::make_unique<RemoteEventObjectChannelServer<std::vector<std::string>>>(m_strategy_names_channel, event_loop));
    m_remote_channel_server.register_channel(
            RemoteChannelNames::symbols,
            std::make_unique<RemoteEventObjectChannelServer<std::vector<Symbol>>>(m_symbols_channel, event_loop));
    m_remote_channel_server.register_channel(
            RemoteChannelNames::start_backtest,
            std::make_unique<RemoteEventChannelServer<StartBacktestCommand>>(m_start_backtest_channel, event_loop));
    m_remote_channel_server.register_channel(
            RemoteChannelNames::stop,
            std::make_unique<RemoteEventChannelServer<StopCommand>>(m_stop_channel, event_loop));
    m_remote_channel_server.register_channel(
            RemoteChannelNames::command_replies,
            std::make_unique<RemoteEventChannelServer<CommandReply>>(m_command_reply_channel, event_loop));
}

void Backend::start_backtest(const StartBacktestCommand & command)
{
    if (m_run_is_active) {
        reply_error(command.command_guid, "A run is active");
        return;
    }
    if (command.start >= command.end || command.start.count() < 0) {
        reply_error(command.command_guid, "Invalid timerange");
        return;
    }
    const auto symbol_opt = find_symbol(command.symbol_name);
    if (!symbol_opt.has_value()) {
        reply_error(command.command_guid, "Invalid symbol on starting strategy: " + command.symbol_name);
        return;
    }

    drop_current_run();
    auto launchpad = std::make_shared<Launchpad>(
            symbol_opt.value(),
            HistoricalMDRequestData{.start = command.start, .end = command.end},
            command.strategy_name,
            command.strategy_config,
            m_gateway);
    launchpad->backtesting_gateway()->push_playback_settings(command.playback_settings);

    start_run(std::move(launchpad), true, command.channel_capacity, command.command_guid);
}

void Backend::start_live_run(const StartLiveRunCommand & command)
{
    if (m_run_is_active) {
        reply_error(command.command_guid, "A run is active");
        return;
    }
    const auto symbol_opt = find_symbol(command.symbol_name);
    if (!symbol_opt.has_value()) {
        reply_error(command.command_guid, "Invalid symbol on starting strategy: " + command.symbol_name);
        return;
    }

    drop_current_run();
    if (!m_trading_gateway) {
        m_trading_gateway = std::make_unique<ByBitTradingGateway>();
    }
    auto launchpad = std::make_shared<Launchpad>(
            symbol_opt.value(),
            command.strategy_name,
            command.strategy_config,
            m_gateway,
            *m_trading_gateway);

    start_run(std::move(launchpad), false, command.channel_capacity, command.command_guid);
}

void Backend::start_run(
        std::shared_ptr<Launchpad> launchpad,
        bool is_backtest,
        std::optional<std::chrono::milliseconds> channel_capacity,
        xg::Guid command_guid)
{
    m_launchpad = std::move(launchpad);
    auto & strategy_instance = m_launchpad->strategy_instance();

    if (channel_capacity.has_value()) {
        strategy_instance.set_channel_capacity(channel_capacity);
    }

    register_run_remote_channels();
    m_run_sub->subscribe(
            strategy_instance.status_channel(),
            [this](const WorkStatus & status) {
                if (status != WorkStatus::Running) {
                    m_run_is_active = false;
                }
            });
    m_run_sub->subscribe(
            strategy_instance.chart_series_list_channel(),
            [this](const ChartSeriesList & list) { register_own_series_remote_channels(list); });

    m_current_run_channel.push({.strategy_instance = Launchpad::share_strategy_instance(m_launchpad), .is_backtest = is_backtest});

    if (strategy_instance.status_channel().get() == WorkStatus::Panic) {
        reply_error(command_guid, "Failed to build strategy");
        return;
    }

    m_run_is_active = true;
    strategy_instance.run_async();
    LOG_STATUS("Strategy started");
    reply_ok(command_guid);
}

void Backend::drop_current_run()
{
    if (!m_launchpad) {
        return;
    }

    m_run_sub = std::make_unique<EventSubcriber>(m_event_loop);
    unregister_run_remote_channels();
    m_current_run_channel.push({});
    m_launchpad.reset();
    m_run_is_active = false;
}

void Backend::stop_run(const StopCommand & command)
{
    if (!m_run_is_active) {
        reply_error(command.command_guid, "No active run");
        return;
    }

    m_launchpad->strategy_instance().stop_async();
    reply_ok(command.command_guid);
}

void Backend::push_playback_settings(const PlaybackSettingsEvent & settings)
{
    UNWRAP_RET_VOID(launchpad, m_launchpad);
    UNWRAP_RET_VOID(backtesting_gateway, launchpad.backtesting_gateway());
    backtesting_gateway.push_playback_settings(settings);
}

void Backend::push_playback_step(const PlaybackStepEvent & step)
{
    UNWRAP_RET_VOID(launchpad, m_launchpad);
    UNWRAP_RET_VOID(backtesting_gateway, launchpad.backtesting_gateway());
    backtesting_gateway.push_playback_step(step);
}

void Backend::start_optimizer(const StartOptimizerCommand & command)
{
    if (m_launchpad) {
        if (m_launchpad->strategy_instance().status_channel().get() != WorkStatus::Stopped) {
            LOG_ERROR("Strategy is not stopped");
            return;
        }
        drop_current_run();
    }

    if (command.start >= command.end || command.start.count() < 0) {
        LOG_ERROR("Invalid timerange");
        return;
    }
    const auto symbol_opt = find_symbol(command.symbol_name);
    if (!symbol_opt.has_value()) {
        LOG_ERROR("Invalid symbol on starting optimizer: {}", command.symbol_name);
        return;
    }

    std::thread t([this, command, symbol = symbol_opt.value()]() {
        Optimizer optimizer(
                m_gateway,
                symbol,
                Timerange{command.start, command.end},
                command.strategy_name,
                command.optimizer_inputs,
                command.thread_count);

        optimizer.subscribe_for_passed_check([this](int passed_checks, int total_checks) {
            m_optimizer_progress_channel.push({.passed_checks = passed_checks, .total_checks = total_checks});
        });

        const auto best_config = optimizer.optimize();
        if (!best_config.has_value()) {
            LOG_INFO("No best config");
            return;
        }
        m_optimized_config_channel.push(best_config.value());
        LOG_INFO("Best config: {}", best_config.value());
    });
    t.detach();
}

template <class ObjectT>
void Backend::register_run_object_channel(const std::string & channel_name, EventObjectChannel<ObjectT> & channel)
{
    m_remote_channel_server.register_channel(
            channel_name,
            std::make_unique<RemoteEventObjectChannelServer<ObjectT>>(channel, m_remote_channel_server.event_loop()));
    m_run_remote_channel_names.insert(channel_name);
}

template <class ObjectT>
void Backend::register_run_timeseries_channel(const std::string & channel_name, EventTimeseriesChannel<ObjectT> & channel)
{
    m_remote_channel_server.register_channel(
            channel_name,
            std::make_unique<RemoteEventTimeseriesChannelServer<ObjectT>>(channel, m_remote_channel_server.event_loop()));
    m_run_remote_channel_names.insert(channel_name);
}

void Backend::register_run_remote_channels()
{
    auto & strategy_instance = m_launchpad->strategy_instance();

    register_run_object_channel(RemoteChannelNames::work_status, strategy_instance.status_channel());
    register_run_object_channel(RemoteChannelNames::strategy_result, strategy_instance.strategy_result_channel());
    register_run_object_channel(RemoteChannelNames::chart_series_list, strategy_instance.chart_series_list_channel());
    register_run_timeseries_channel(RemoteChannelNames::positions, strategy_instance.positions_channel());

    register_run_timeseries_channel(RemoteChannelNames::series(FixedChartSeries::candles), strategy_instance.candle_channel());
    register_run_timeseries_channel(RemoteChannelNames::series(FixedChartSeries::trades), strategy_instance.trade_channel());
    register_run_timeseries_channel(RemoteChannelNames::series(FixedChartSeries::price_levels), strategy_instance.price_levels_channel());
    register_run_timeseries_channel(RemoteChannelNames::series(FixedChartSeries::tpsl), strategy_instance.tpsl_channel());
    register_run_timeseries_channel(RemoteChannelNames::series(FixedChartSeries::trailing_stop), strategy_instance.trailing_stop_channel());
    register_run_timeseries_channel(RemoteChannelNames::series(FixedChartSeries::market_states), strategy_instance.market_state_channel());
    register_run_timeseries_channel(RemoteChannelNames::series(FixedChartSeries::depo), strategy_instance.depo_channel());
}

void Backend::register_own_series_remote_channels(const ChartSeriesList & list)
{
    auto & strategy_instance = m_launchpad->strategy_instance();

    for (const auto & name : list.series_names) {
        UNWRAP_CONTINUE(own_series_channel, strategy_instance.find_own_series_channel(name));
        if (m_run_remote_channel_names.contains(RemoteChannelNames::series(name))) {
            continue;
        }
        register_run_timeseries_channel(RemoteChannelNames::series(name), own_series_channel);
    }
}

void Backend::unregister_run_remote_channels()
{
    for (const auto & channel_name : m_run_remote_channel_names) {
        m_remote_channel_server.unregister_channel(channel_name);
    }
    m_run_remote_channel_names.clear();
}

std::optional<Symbol> Backend::find_symbol(const std::string & symbol_name)
{
    for (const auto & symbol : m_symbols_channel.get()) {
        if (symbol.symbol_name == symbol_name) {
            return symbol;
        }
    }
    return std::nullopt;
}

void Backend::reply_ok(xg::Guid command_guid)
{
    m_command_reply_channel.push({.command_guid = command_guid, .error = std::nullopt});
}

void Backend::reply_error(xg::Guid command_guid, const std::string & error)
{
    LOG_ERROR("{}", error);
    m_command_reply_channel.push({.command_guid = command_guid, .error = error});
}
