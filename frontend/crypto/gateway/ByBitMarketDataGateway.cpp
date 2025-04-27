#include "ByBitMarketDataGateway.h"

#include "BybitTradesDownloader.h"
#include "Logger.h"
#include "MarketDataMessages.h"
#include "Ohlc.h"
#include "ScopeExit.h"
#include "Symbol.h"

#include <chrono>
#include <crossguid/guid.hpp>
#include <future>
#include <string>
#include <vector>

ByBitMarketDataGateway::ByBitMarketDataGateway(bool start)
    : m_connection_watcher(*this)
{
    const auto config_opt = GatewayConfigLoader::load();
    if (!config_opt) {
        Logger::log<LogLevel::Error>("Failed to load bybit trading gateway config");
        return;
    }
    m_config = config_opt.value().market_data;

    register_invokers();

    if (!start) {
        return;
    }

    m_last_server_time = get_server_time();

    if (!reconnect_ws_client()) {
        Logger::log<LogLevel::Warning>("Failed to connect to ByBit market data");
    }
}

bool ByBitMarketDataGateway::reconnect_ws_client()
{
    m_ws_client = std::make_shared<WebSocketClient>(
            std::string(m_config.ws_url),
            std::nullopt,
            [this](const json & j) {
                on_price_received(j);
            },
            m_connection_watcher);

    if (!m_ws_client->wait_until_ready()) {
        return false;
    }

    auto weak_ptr = std::weak_ptr<IPingSender>(m_ws_client);
    m_connection_watcher.set_ping_sender(weak_ptr);

    {
        auto lref = m_live_requests.lock();
        for (auto & it : lref.get()) {
            Logger::logf<LogLevel::Debug>("MD Subscribing to: {}", it.symbol.symbol_name);
            m_ws_client->subscribe("publicTrade." + it.symbol.symbol_name);
        }
    }

    m_event_loop.push_delayed(ws_ping_interval, PingCheckEvent{});
    return true;
}

struct OhlcResult
{
    std::string category;
    std::string symbol;
    std::vector<OHLC> ohlc_list;
};

OHLC from_strings(const std::string & timestamp,
                  const std::string & open,
                  const std::string & high,
                  const std::string & low,
                  const std::string & close,
                  const std::string & volume,
                  const std::string & turnover)
{
    return OHLC{.timestamp = std::chrono::milliseconds{std::stoull(timestamp)},
                .open = std::stod(open),
                .high = std::stod(high),
                .low = std::stod(low),
                .close = std::stod(close),
                .volume = std::stod(volume),
                .turnover = std::stod(turnover)};
}

void from_json(const json & j, OhlcResult & result)
{
    j.at("category").get_to(result.category);
    j.at("symbol").get_to(result.symbol);

    result.ohlc_list.clear();
    for (const auto & ohlc_list : j.at("list")) {
        auto ohlc = from_strings(ohlc_list[0].get<std::string>(),
                                 ohlc_list[1].get<std::string>(),
                                 ohlc_list[2].get<std::string>(),
                                 ohlc_list[3].get<std::string>(),
                                 ohlc_list[4].get<std::string>(),
                                 ohlc_list[5].get<std::string>(),
                                 ohlc_list[6].get<std::string>());
        result.ohlc_list.push_back(ohlc);
    }
}

struct OhlcResponse
{
    int ret_code = {};
    std::string ret_msg;
    OhlcResult result;
};

void from_json(const json & j, OhlcResponse & response)
{
    j.at("retCode").get_to(response.ret_code);
    j.at("retMsg").get_to(response.ret_msg);
    const auto & j2 = j["result"];
    j2.get_to(response.result);
}

struct ServerTimeResponse
{
    struct Result
    {
        std::chrono::seconds time_sec;
        std::chrono::nanoseconds time_nano;
    };

    Result result;
};

void from_json(const json & j, ServerTimeResponse::Result & result)
{
    std::string time_sec_str = {};
    j.at("timeSecond").get_to(time_sec_str);

    std::string time_nano_str = {};
    j.at("timeNano").get_to(time_nano_str);

    uint64_t time_sec = std::stoull(time_sec_str);
    result.time_sec = std::chrono::seconds{time_sec};
    uint64_t time_nano = std::stoull(time_nano_str);
    result.time_nano = std::chrono::nanoseconds{time_nano};
}

void from_json(const json & j, ServerTimeResponse & response)
{
    const auto & j2 = j["result"];
    j2.get_to(response.result);
}

struct SymbolResponse
{
    struct Result
    {
        std::string category;
        std::vector<Symbol> symbol_vec;
    };

    int ret_code = {};
    std::string ret_msg;
    Result result;
};

void from_json(const json & j, Symbol::LotSizeFilter & size_filter)
{
    size_filter.max_qty = std::stod(j.at("maxOrderQty").get<std::string>());
    size_filter.min_qty = std::stod(j.at("minOrderQty").get<std::string>());
    size_filter.qty_step = std::stod(j.at("qtyStep").get<std::string>());
}

void from_json(const json & j, Symbol & symbol)
{
    j.at("symbol").get_to(symbol.symbol_name);
    j.at("lotSizeFilter").get_to(symbol.lot_size_filter);
}

void from_json(const json & j, SymbolResponse::Result & symbol_result)
{
    j.at("category").get_to(symbol_result.category);
    j.at("list").get_to(symbol_result.symbol_vec);
}

void from_json(const json & j, SymbolResponse & response)
{
    j.at("retCode").get_to(response.ret_code);
    j.at("retMsg").get_to(response.ret_msg);
    const auto & j2 = j["result"];
    j2.get_to(response.result);
}

bool ByBitMarketDataGateway::request_historical_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && pack_callback)
{
    Logger::logf<LogLevel::Debug>(
            "Whole requested interval seconds: {}",
            std::chrono::duration_cast<std::chrono::seconds>(timerange.duration()).count());

    auto last_start = timerange.start();
    while (last_start < timerange.end()) {
        const unsigned limit = 1000;
        const auto end = [&]() -> std::chrono::milliseconds {
            const auto possible_end = last_start + min_historical_interval * 1000;
            if (timerange.end() < possible_end) {
                return timerange.end();
            }
            else {
                return possible_end;
            }
        }();

        ScopeExit se([&]() {
            last_start = end + min_historical_interval;
        });

        const std::chrono::milliseconds remaining_delta = timerange.end() - last_start;
        Logger::logf<LogLevel::Debug>("Remaining time delta: {}ms", remaining_delta.count());

        const std::string category = "linear";

        const std::string request = [&]() {
            std::stringstream ss;
            ss << m_config.rest_url
               << "/v5/market/kline"
               << "?symbol=" << symbol
               << "&category=" << category
               << "&interval=" << min_historical_interval.count()
               << "&limit=" << limit
               << "&start=" << last_start.count()
               << "&end=" << end.count();
            return std::string(ss.str());
        }();

        auto future = rest_client.request_async(request);
        future.wait();
        OhlcResponse response;
        const auto j = json::parse(future.get());
        from_json(j, response);

        std::map<std::chrono::milliseconds, OHLC> inter_result;
        for (const auto & ohlc : response.result.ohlc_list) {
            inter_result.try_emplace(ohlc.timestamp, ohlc);
        }

        Logger::logf<LogLevel::Debug>("Got {} prices", inter_result.size());
        if (inter_result.empty()) {
            Logger::log<LogLevel::Warning>("Empty kline result");
            return false;
        }
        if (const auto delta = inter_result.begin()->first - last_start; delta > min_historical_interval) {
            Logger::logf<LogLevel::Error>(
                    "ERROR inconsistent time delta: {}s. Stopping",
                    std::chrono::duration_cast<std::chrono::seconds>(delta).count());

            Logger::logf<LogLevel::Error>(
                    "First timestamp: {}s",
                    std::chrono::duration_cast<std::chrono::seconds>(inter_result.begin()->first).count());
            return false;
        }
        pack_callback(std::move(inter_result));
    }

    Logger::logf<LogLevel::Debug>(
            "All prices received for interval: {}-{}",
            timerange.start().count(),
            timerange.end().count());
    return true;
}

std::vector<Symbol> ByBitMarketDataGateway::get_symbols(const std::string & currency)
{
    const std::string category = "linear";
    const unsigned limit = 1000;

    const std::string url = [&]() {
        std::stringstream ss;
        ss << m_config.rest_url
           << "/v5/market/instruments-info"
           << "?category=" << category
           << "&limit=" << limit;
        return std::string(ss.str());
    }();

    auto str_future = rest_client.request_async(url);
    // TODO set timeout
    str_future.wait();

    SymbolResponse response;
    const auto j = json::parse(str_future.get());
    j.get_to(response);

    response.result.symbol_vec.erase(std::remove_if(
                                             response.result.symbol_vec.begin(),
                                             response.result.symbol_vec.end(),
                                             [&currency](const Symbol & symbol) {
                                                 return !symbol.symbol_name.ends_with(currency);
                                             }),
                                     response.result.symbol_vec.end());

    Logger::logf<LogLevel::Info>("Got {} symbols with currency {}", response.result.symbol_vec.size(), currency);
    return response.result.symbol_vec;
}

std::chrono::milliseconds ByBitMarketDataGateway::get_server_time()
{
    const std::string url = std::string(m_config.rest_url) + "/v5/market/time";

    auto str_future = rest_client.request_async(url);
    str_future.wait();

    ServerTimeResponse response{};
    const auto j = json::parse(str_future.get()); // TODO there can be an empty string
    j.get_to(response);

    const auto server_time = std::chrono::duration_cast<std::chrono::milliseconds>(response.result.time_nano);
    Logger::logf<LogLevel::Debug>("Server time: {}", server_time.count());
    return server_time;
}

EventObjectChannel<WorkStatus> & ByBitMarketDataGateway::status_channel()
{
    return m_status;
}

void ByBitMarketDataGateway::push_async_request(HistoricalMDRequest && request)
{
    m_event_loop.push_event(request);
}

void ByBitMarketDataGateway::push_async_request(LiveMDRequest && request)
{
    m_event_loop.push_event(request);
}

void ByBitMarketDataGateway::handle_request(const HistoricalMDRequest & request)
{
    const auto reader_ptr = BybitTradesDownloader::request(request);
    HistoricalMDGeneratorEvent ev(request.guid, reader_ptr);
    m_historical_prices_channel.push(ev);
}

void ByBitMarketDataGateway::handle_request(const LiveMDRequest & request)
{
    const LiveMDRequest & live_request = request;
    auto locked_ref = m_live_requests.lock();
    if (!locked_ref.get().empty()) {
        Logger::log<LogLevel::Error>("More than one MD live request");
        return;
    }

    if (!m_ws_client) {
        Logger::log<LogLevel::Error>("websocket is not ready");
        return;
    }

    m_ws_client->subscribe("publicTrade." + live_request.symbol.symbol_name);
    locked_ref.get().push_back(live_request);
}

void ByBitMarketDataGateway::handle_request(const PingCheckEvent & event)
{
    m_connection_watcher.handle_request(event);
}

void ByBitMarketDataGateway::on_price_received(const nlohmann::json & json)
{
    auto locked_ref = m_live_requests.lock();
    if (locked_ref.get().empty()) {
        Logger::log<LogLevel::Error>("no request on MD received");
    }

    const auto trades_list = json.get<PublicTradeList>();
    for (const auto & trade : trades_list.trades) {
        OHLC ohlc = {.timestamp = trade.timestamp, .open = trade.price, .high = trade.price, .low = trade.price, .close = trade.price};
        // TODO pushing only close price is not quite correct
        MDPriceEvent ev{{trade.timestamp, ohlc.close, SignedVolume{0.}}}; // TODO
        m_live_prices_channel.push(ev);
    }
}

void ByBitMarketDataGateway::register_invokers()
{
    m_invoker_subs.push_back(
            m_event_loop.invoker().register_invoker<HistoricalMDRequest>(
                    [&](const auto & r) { handle_request(r); }));

    m_invoker_subs.push_back(
            m_event_loop.invoker().register_invoker<LiveMDRequest>(
                    [&](const auto & r) { handle_request(r); }));

    m_invoker_subs.push_back(
            m_event_loop.invoker().register_invoker<PingCheckEvent>(
                    [&](const auto & r) { handle_request(r); }));
}

void ByBitMarketDataGateway::unsubscribe_from_live(xg::Guid guid)
{
    auto live_req_locked = m_live_requests.lock();
    for (auto it = live_req_locked.get().begin(), end = live_req_locked.get().end(); it != end; ++it) {
        if (it->guid == guid) {
            Logger::logf<LogLevel::Debug>("Erasing live request: {}", guid);
            if (m_ws_client) {
                m_ws_client->unsubscribe("publicTrade." + it->symbol.symbol_name);
            }
            live_req_locked.get().erase(it);
            break;
        }
    }
}

void ByBitMarketDataGateway::on_connection_lost()
{
    Logger::log<LogLevel::Warning>("Connection lost on market data, reconnecting...");
    if (!reconnect_ws_client()) {
        Logger::log<LogLevel::Warning>("Failed to connect to ByBit market data");
        m_event_loop.push_delayed(std::chrono::seconds{30}, PingCheckEvent{});
    }
}

void ByBitMarketDataGateway::on_connection_verified()
{
    m_event_loop.push_delayed(ws_ping_interval, PingCheckEvent{});
}

EventChannel<HistoricalMDGeneratorEvent> & ByBitMarketDataGateway::historical_prices_channel()
{
    return m_historical_prices_channel;
}

EventChannel<MDPriceEvent> & ByBitMarketDataGateway::live_prices_channel() { return m_live_prices_channel; }
