#include "BackendCommands.h"
#include "CandleJsonCodec.h"
#include "ChartSeriesListJsonCodec.h"
#include "CommandReplyJsonCodec.h"
#include "ContainerJsonCodec.h"
#include "EventLoop.h"
#include "EventLoopSubscriber.h"
#include "MarketStateRenderObjectJsonCodec.h"
#include "PositionResultJsonCodec.h"
#include "PrimitiveJsonCodec.h"
#include "ProfitPriceLevelsJsonCodec.h"
#include "RemoteChannelClient.h"
#include "RemoteChannelNames.h"
#include "RemoteEventChannel.h"
#include "RemoteEventObjectChannel.h"
#include "RemoteEventTimeseriesChannel.h"
#include "StartBacktestCommandJsonCodec.h"
#include "StopCommandJsonCodec.h"
#include "StopLossJsonCodec.h"
#include "StrategyResultJsonCodec.h"
#include "SymbolJsonCodec.h"
#include "TimeseriesPointJsonCodec.h"
#include "TpslPricesJsonCodec.h"
#include "TradeJsonCodec.h"
#include "WorkStatusJsonCodec.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::string remote_host = "127.0.0.1";

const std::string general_help =
        R"(Usage: crypto-cli <command> [options]

Commands:
  strategies   List strategy names
  symbols      List symbol names
  start        Start a backtest
  stop         Stop the run
  status       Work status of the run
  result       Strategy result of the run
  positions    Positions of the run
  charts       List of charts and their series of the run
  series       Points of one series within a time interval
  help         Help for a command

The output of every command is JSON.
'crypto-cli help <command>' or 'crypto-cli <command> --help' prints the help for a command.
)";

const std::map<std::string, std::string> command_helps = {
        {"strategies",
         R"(Usage: crypto-cli strategies

Prints the list of strategy names.
)"},
        {"symbols",
         R"(Usage: crypto-cli symbols

Prints the list of symbol names.
)"},
        {"start",
         R"(Usage: crypto-cli start --symbol <name> --strategy <name> --config <file> --from <time> --to <time> [--capacity-hours <n>]

Starts a backtest in 'Rapid' mode. It is rejected while a run is active.

  --symbol <name>         Symbol name, see 'crypto-cli symbols'
  --strategy <name>       Strategy name, see 'crypto-cli strategies'
  --config <file>         JSON file with the parameter values of the strategy
  --from <time>           Start of the backtest
  --to <time>             End of the backtest
  --capacity-hours <n>    How many hours of history the channels of the run keep

<time> is milliseconds since 1970, or a date and time in UTC like '2026-01-31T12:00:00'.
)"},
        {"stop",
         R"(Usage: crypto-cli stop

Asks the active run to stop. The run stops a bit later, 'crypto-cli status' shows it.
)"},
        {"status",
         R"(Usage: crypto-cli status [--live]

Prints the work status of the run: 'Stopped', 'Running' or 'Panic'.

  --live    Stay connected and print every new status
)"},
        {"result",
         R"(Usage: crypto-cli result [--live]

Prints the strategy result of the run.

  --live    Stay connected and print every new result
)"},
        {"positions",
         R"(Usage: crypto-cli positions [--live]

Prints the list of positions of the run.

  --live    Stay connected and print every new position
)"},
        {"charts",
         R"(Usage: crypto-cli charts

Prints the list of charts and their series of the run.
)"},
        {"series",
         R"(Usage: crypto-cli series <chart> <series> --from <time> --to <time> [--live]

Prints the points of one series within a time interval.

  <chart> <series>    Names from 'crypto-cli charts'
  --from <time>       Start of the interval
  --to <time>         End of the interval
  --live              Stay connected and print every new point

<time> is milliseconds since 1970, or a date and time in UTC like '2026-01-31T12:00:00'.
)"},
        {"help",
         R"(Usage: crypto-cli help [<command>]

Prints the help for a command, or the list of commands.
)"},
};

struct CommandLine
{
    std::string command;
    std::vector<std::string> positional_arguments;
    std::map<std::string, std::string> options;
    bool live = false;
    bool help = false;
};

CommandLine parse_command_line_throw(int argc, char * argv[])
{
    const std::set<std::string> option_names = {"--symbol", "--strategy", "--config", "--from", "--to", "--capacity-hours"};

    CommandLine command_line;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--live") {
            command_line.live = true;
        }
        else if (argument == "--help") {
            command_line.help = true;
        }
        else if (option_names.contains(argument)) {
            if (i + 1 >= argc) {
                throw std::runtime_error("No value for option " + argument);
            }
            command_line.options[argument] = argv[++i];
        }
        else if (argument.starts_with("--")) {
            throw std::runtime_error("Unknown option " + argument);
        }
        else if (command_line.command.empty()) {
            command_line.command = argument;
        }
        else {
            command_line.positional_arguments.push_back(argument);
        }
    }
    return command_line;
}

std::string get_required_option_throw(const CommandLine & command_line, const std::string & option_name)
{
    const auto it = command_line.options.find(option_name);
    if (it == command_line.options.end()) {
        throw std::runtime_error("Option " + option_name + " is required");
    }
    return it->second;
}

std::chrono::milliseconds parse_time_throw(const std::string & text)
{
    const bool is_milliseconds = !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); });
    if (is_milliseconds) {
        return std::chrono::milliseconds{std::stoll(text)};
    }

    std::tm date_time = {};
    std::istringstream stream(text);
    stream >> std::get_time(&date_time, "%Y-%m-%dT%H:%M:%S");
    if (stream.fail()) {
        throw std::runtime_error("Wrong time: " + text);
    }
    return std::chrono::seconds{timegm(&date_time)};
}

JsonStrategyConfig read_strategy_config_throw(const std::string & file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Can't open config file: " + file_path);
    }
    return JsonStrategyConfig{nlohmann::json::parse(file)};
}

std::mutex output_mutex;

template <class ObjectT>
void print_pretty(const ObjectT & object)
{
    std::cout << JsonCodec<ObjectT>::encode(object).dump(4) << std::endl;
}

void wait_for_stream_end_throw(RemoteChannelClient & client)
{
    if (client.wait_for_stream_end() == RemoteStreamEnd::ConnectionLost) {
        throw std::runtime_error("Connection is lost");
    }
}

template <class ObjectT>
void print_object_channel_throw(RemoteChannelClient & client, const std::string & channel_name, bool live)
{
    RemoteEventObjectChannel<ObjectT> channel(client, channel_name);
    if (!live) {
        print_pretty(channel.get());
        return;
    }

    EventLoop event_loop;
    EventSubcriber subscriber(event_loop);
    {
        std::lock_guard lock(output_mutex);
        subscriber.subscribe(
                channel,
                [](const ObjectT & object) {
                    std::lock_guard update_lock(output_mutex);
                    print_pretty(object);
                });
        print_pretty(channel.get());
    }
    wait_for_stream_end_throw(client);
}

void print_symbol_names_throw(RemoteChannelClient & client)
{
    RemoteEventObjectChannel<std::vector<Symbol>> channel(client, RemoteChannelNames::symbols);

    std::vector<std::string> symbol_names;
    for (const auto & symbol : channel.get()) {
        symbol_names.push_back(symbol.symbol_name);
    }
    print_pretty(symbol_names);
}

void print_positions_throw(RemoteChannelClient & client, bool live)
{
    using PointT = std::pair<std::chrono::milliseconds, PositionResult>;

    RemoteEventTimeseriesChannel<PositionResult> channel(client, RemoteChannelNames::positions);
    if (!live) {
        print_pretty(channel.data_copy());
        return;
    }

    EventLoop event_loop;
    EventSubcriber subscriber(event_loop);
    subscriber.subscribe(
            channel,
            [](const std::deque<PointT> & points) { print_pretty(points); },
            [](std::chrono::milliseconds ts, const PositionResult & position_result) {
                print_pretty(PointT{ts, position_result});
            });
    wait_for_stream_end_throw(client);
}

template <class ObjectT>
void print_series_throw(
        RemoteChannelClient & client,
        const ChartSeriesName & name,
        std::chrono::milliseconds start,
        std::chrono::milliseconds end,
        bool live)
{
    using PointT = std::pair<std::chrono::milliseconds, ObjectT>;

    RemoteEventTimeseriesChannel<ObjectT> channel(client, RemoteChannelNames::series(name));
    if (!live) {
        print_pretty(channel.data_copy_in_range(start, end));
        return;
    }

    EventLoop event_loop;
    EventSubcriber subscriber(event_loop);
    {
        std::lock_guard lock(output_mutex);
        subscriber.subscribe(
                channel,
                {},
                [](std::chrono::milliseconds ts, const ObjectT & object) {
                    std::lock_guard update_lock(output_mutex);
                    print_pretty(PointT{ts, object});
                });
        print_pretty(channel.data_copy_in_range(start, end));
    }
    wait_for_stream_end_throw(client);
}

void print_series_by_name_throw(RemoteChannelClient & client, const CommandLine & command_line)
{
    if (command_line.positional_arguments.size() != 2) {
        throw std::runtime_error("Chart and series names are required");
    }
    const ChartSeriesName name{.chart_name = command_line.positional_arguments[0], .series_name = command_line.positional_arguments[1]};
    const auto start = parse_time_throw(get_required_option_throw(command_line, "--from"));
    const auto end = parse_time_throw(get_required_option_throw(command_line, "--to"));
    const bool live = command_line.live;

    if (name == FixedChartSeries::candles) {
        print_series_throw<Candle>(client, name, start, end, live);
    }
    else if (name == FixedChartSeries::trades) {
        print_series_throw<Trade>(client, name, start, end, live);
    }
    else if (name == FixedChartSeries::price_levels) {
        print_series_throw<ProfitPriceLevels>(client, name, start, end, live);
    }
    else if (name == FixedChartSeries::tpsl) {
        print_series_throw<TpslPrices>(client, name, start, end, live);
    }
    else if (name == FixedChartSeries::trailing_stop) {
        print_series_throw<StopLoss>(client, name, start, end, live);
    }
    else if (name == FixedChartSeries::market_states) {
        print_series_throw<MarketStateRenderObject>(client, name, start, end, live);
    }
    else {
        print_series_throw<double>(client, name, start, end, live);
    }
}

template <class CommandT>
void execute_command_throw(RemoteChannelClient & client, const std::string & channel_name, const CommandT & command)
{
    RemoteEventChannel<CommandReply> reply_channel(client, RemoteChannelNames::command_replies);
    RemoteEventChannel<CommandT> command_channel(client, channel_name);

    std::promise<CommandReply> reply_promise;
    auto reply_future = reply_promise.get_future();

    EventLoop event_loop;
    EventSubcriber subscriber(event_loop);
    subscriber.subscribe(
            reply_channel,
            [&](const CommandReply & reply) {
                if (reply.command_guid == command.command_guid) {
                    reply_promise.set_value(reply);
                }
            });
    command_channel.push(command);

    while (reply_future.wait_for(std::chrono::milliseconds{100}) != std::future_status::ready) {
        if (client.connection_is_lost()) {
            throw std::runtime_error("Connection is lost");
        }
    }

    const auto reply = reply_future.get();
    if (reply.error.has_value()) {
        throw std::runtime_error(reply.error.value());
    }
    std::cout << nlohmann::json{{"status", "ok"}}.dump(4) << std::endl;
}

StartBacktestCommand make_start_backtest_command_throw(const CommandLine & command_line)
{
    std::optional<std::chrono::milliseconds> channel_capacity;
    if (command_line.options.contains("--capacity-hours")) {
        channel_capacity = std::chrono::hours{std::stoll(command_line.options.at("--capacity-hours"))};
    }

    return {
            .command_guid = xg::newGuid(),
            .symbol_name = get_required_option_throw(command_line, "--symbol"),
            .strategy_name = get_required_option_throw(command_line, "--strategy"),
            .strategy_config = read_strategy_config_throw(get_required_option_throw(command_line, "--config")),
            .start = parse_time_throw(get_required_option_throw(command_line, "--from")),
            .end = parse_time_throw(get_required_option_throw(command_line, "--to")),
            .channel_capacity = channel_capacity,
            .playback_settings = PlaybackSettingsEvent{PlaybackMode::Rapid, std::chrono::minutes{1}},
    };
}

void print_help_throw(const std::string & command)
{
    if (command.empty()) {
        std::cout << general_help;
        return;
    }
    const auto it = command_helps.find(command);
    if (it == command_helps.end()) {
        throw std::runtime_error("Unknown command: " + command);
    }
    std::cout << it->second;
}

void run_command_throw(const CommandLine & command_line)
{
    const auto & command = command_line.command;

    if (command.empty()) {
        print_help_throw("");
        return;
    }
    if (command == "help") {
        print_help_throw(command_line.positional_arguments.empty() ? "" : command_line.positional_arguments.front());
        return;
    }
    if (!command_helps.contains(command)) {
        throw std::runtime_error("Unknown command: " + command);
    }
    if (command_line.help) {
        print_help_throw(command);
        return;
    }

    RemoteChannelClient client(remote_host, remote_channel_port);

    if (command == "strategies") {
        print_object_channel_throw<std::vector<std::string>>(client, RemoteChannelNames::strategy_names, false);
    }
    else if (command == "symbols") {
        print_symbol_names_throw(client);
    }
    else if (command == "start") {
        execute_command_throw(client, RemoteChannelNames::start_backtest, make_start_backtest_command_throw(command_line));
    }
    else if (command == "stop") {
        execute_command_throw(client, RemoteChannelNames::stop, StopCommand{xg::newGuid()});
    }
    else if (command == "status") {
        print_object_channel_throw<WorkStatus>(client, RemoteChannelNames::work_status, command_line.live);
    }
    else if (command == "result") {
        print_object_channel_throw<StrategyResult>(client, RemoteChannelNames::strategy_result, command_line.live);
    }
    else if (command == "positions") {
        print_positions_throw(client, command_line.live);
    }
    else if (command == "charts") {
        print_object_channel_throw<ChartSeriesList>(client, RemoteChannelNames::chart_series_list, false);
    }
    else if (command == "series") {
        print_series_by_name_throw(client, command_line);
    }
}

} // namespace

int main(int argc, char * argv[])
{
    try {
        run_command_throw(parse_command_line_throw(argc, argv));
        return 0;
    }
    catch (const std::exception & e) {
        std::cout << nlohmann::json{{"error", e.what()}}.dump(4) << std::endl;
        return 1;
    }
}
