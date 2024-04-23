#include "ByBitGateway.h"

#include "MarketDataMessages.h"
#include "Ohlc.h"
#include "ScopeExit.h"
#include "Symbol.h"
#include "WorkerThread.h"

#include <chrono>
#include <crossguid2/crossguid/guid.hpp>
#include <future>
#include <string>
#include <thread>
#include <vector>

ByBitGateway::ByBitGateway()
    : m_event_loop(*this)
    , ws_client(
              std::string(s_test_ws_linear_endpoint_address),
              std::nullopt,
              [this](const json & j) {
                  on_price_received(j);
              })
{
    m_last_server_time = get_server_time();
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
    return OHLC{std::chrono::milliseconds{std::stoull(timestamp)},
                std::stod(open),
                std::stod(high),
                std::stod(low),
                std::stod(close),
                std::stod(volume),
                std::stod(turnover)};
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
    std::string result_raw;
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
    std::string result_raw;
    const auto & j2 = j["result"];
    j2.get_to(response.result);
}

bool ByBitGateway::request_historical_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && pack_callback)
{
    std::cout << "Whole requested interval seconds: " << std::chrono::duration_cast<std::chrono::seconds>(timerange.duration()).count() << std::endl;

    auto last_start = timerange.start();
    while (last_start < timerange.end()) {
        const unsigned limit = 1000;
        const auto end = [&]() -> std::chrono::milliseconds {
            const auto possible_end = last_start + min_interval * 1000;
            if (timerange.end() < possible_end) {
                return timerange.end();
            }
            else {
                return possible_end;
            }
        }();

        ScopeExit se([&]() {
            last_start = end + min_interval;
        });

        const std::chrono::milliseconds remaining_delta = timerange.end() - last_start;
        std::cout << "Remaining time delta: " << remaining_delta.count() << "ms" << std::endl;

        const std::string category = "linear";

        const std::string request = [&]() {
            std::stringstream ss;
            ss << s_endpoint_address
               << "/v5/market/kline"
               << "?symbol=" << symbol
               << "&category=" << category
               << "&interval=" << min_interval.count()
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

        std::cout << "Got " << inter_result.size() << " prices" << std::endl;
        if (inter_result.empty()) {
            std::cout << "Empty kline result" << std::endl;
            return false;
        }
        if (const auto delta = inter_result.begin()->first - last_start; delta > min_interval) {
            std::cout << "ERROR inconsistent time delta: " << std::chrono::duration_cast<std::chrono::seconds>(delta).count() << "s. Stopping" << std::endl;
            std::cout << "First timestamp: " << inter_result.begin()->first.count() << std::endl;
            return false;
        }
        pack_callback(std::move(inter_result));
    }

    std::cout << "All prices received for interval: " << timerange.start().count() << "-" << timerange.end().count() << std::endl;
    return true;
}

std::vector<Symbol> ByBitGateway::get_symbols(const std::string & currency)
{
    const std::string category = "linear";
    const unsigned limit = 1000;

    const std::string url = [&]() {
        std::stringstream ss;
        ss << s_endpoint_address
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

    response.result.symbol_vec.erase(
            std::remove_if(
                    response.result.symbol_vec.begin(),
                    response.result.symbol_vec.end(),
                    [&currency](const Symbol & symbol) {
                        return !symbol.symbol_name.ends_with(currency);
                    }),
            response.result.symbol_vec.end());

    std::cout << "Got " << response.result.symbol_vec.size() << " symbols with currency " << currency << std::endl;
    return response.result.symbol_vec;
}

std::chrono::milliseconds ByBitGateway::get_server_time()
{
    const std::string url = std::string(s_endpoint_address) + "/v5/market/time";

    auto str_future = rest_client.request_async(url);
    str_future.wait();

    ServerTimeResponse response{};
    const auto j = json::parse(str_future.get());
    j.get_to(response);

    const auto server_time = std::chrono::duration_cast<std::chrono::milliseconds>(response.result.time_nano);
    std::cout << "Server time: " << server_time.count() << std::endl;
    return server_time;
}

ObjectPublisher<WorkStatus> & ByBitGateway::status_publisher()
{
    return m_status;
}

void ByBitGateway::wait_for_finish()
{
    if (m_backtest_thread) {
        m_backtest_thread->wait_for_finish();
    }
    if (m_live_thread) {
        m_live_thread->wait_for_finish();
    }
}

void ByBitGateway::stop_async()
{
    if (m_backtest_thread) {
        m_backtest_thread->stop_async();
    }
    if (m_live_thread) {
        m_live_thread->stop_async();
    }
}

void ByBitGateway::push_async_request(HistoricalMDRequest && request)
{
    m_event_loop.as_consumer<HistoricalMDRequest>().push(request);
}

void ByBitGateway::push_async_request(LiveMDRequest && request)
{
    m_event_loop.as_consumer<LiveMDRequest>().push(request);
}

void ByBitGateway::handle_request_deprecated(const LiveMDRequest & request)
{
    std::cout << "Got live request" << std::endl;
    const LiveMDRequest & live_request = request;
    auto locked_ref = m_live_requests.lock();
    locked_ref.get().push_back(live_request);
    if (locked_ref.get().size() == 1) {
        m_live_thread = std::make_unique<WorkerThreadLoop>(
                [this,
                 symbol = live_request.symbol,
                 last_ts = m_last_server_time](const std::atomic_bool & running) mutable -> bool {
                    const auto timerange = Timerange{last_ts + std::chrono::seconds{1}, last_ts + min_interval};

                    request_historical_klines(
                            symbol.symbol_name,
                            timerange,
                            [symbol,
                             &last_ts,
                             this](std::map<std::chrono::milliseconds, OHLC> && ts_and_ohlc_map) {
                                for (const auto & ts_ohlc_pair : ts_and_ohlc_map) {
                                    auto requests = m_live_requests.lock();
                                    for (const auto & req : requests.get()) {
                                        if (req.symbol.symbol_name == symbol.symbol_name) {
                                            MDPriceEvent ev;
                                            ev.ts_and_price = ts_ohlc_pair;
                                            req.event_consumer->push(ev);
                                        }
                                    }
                                    last_ts = ts_and_ohlc_map.rbegin()->first;
                                    std::cout << "ts: " << ts_ohlc_pair.first.count() << "OHLC: " << ts_ohlc_pair.second << std::endl;
                                }
                            });

                    const auto wait_time = std::chrono::seconds{30};
                    std::cout << "Got all live prices, sleeping for " << wait_time.count() << " seconds" << std::endl;
                    const auto now = std::chrono::steady_clock::now();
                    while (running && std::chrono::steady_clock::now() < now + wait_time) {
                        std::this_thread::sleep_for(std::chrono::milliseconds{1});
                    }

                    return true;
                });
        m_live_thread->set_on_finish([this]() {
            m_status.push(WorkStatus::Stopped);
            std::cout << "Stopping live connection" << std::endl;
        });
    }
}

void ByBitGateway::handle_request(const LiveMDRequest & request)
{
    const LiveMDRequest & live_request = request;
    auto locked_ref = m_live_requests.lock();
    if (!locked_ref.get().empty()) {
        std::cout << "ERROR! more than one MD live request" << std::endl;
        return;
    }

    if (!ws_client.wait_until_ready()) {
        std::cout << "ERROR! websocket not ready" << std::endl;
        return;
    }

    ws_client.subscribe("publicTrade." + live_request.symbol.symbol_name);
    locked_ref.get().push_back(live_request);
}

void ByBitGateway::on_price_received(const nlohmann::json & json)
{
    auto locked_ref = m_live_requests.lock();
    if (locked_ref.get().empty()) {
        std::cout << "ERROR! no request on MD received" << std::endl;
        return;
    }

    const LiveMDRequest & live_request = locked_ref.get().front();
    const auto trades_list = json.get<PublicTradeList>();
    for (const auto & trade : trades_list.trades) {
        OHLC ohlc = {trade.timestamp, trade.price, trade.price, trade.price, trade.price};
        MDPriceEvent ev;
        ev.ts_and_price = {trade.timestamp, ohlc};
        live_request.event_consumer->push(ev);
    }
}

void ByBitGateway::invoke(const MDRequest & request)
{
    if (const auto * r = std::get_if<HistoricalMDRequest>(&request); r) {
        const HistoricalMDRequest & histroical_request = *r;
        const auto symbol = histroical_request.symbol;
        const auto histroical_timerange = Timerange{histroical_request.start, histroical_request.end};

        if (auto range_it = m_ranges_by_symbol.find(symbol.symbol_name); range_it != m_ranges_by_symbol.end()) {
            if (auto it = range_it->second.find(histroical_timerange); it != range_it->second.end()) {
                const auto & prices = it->second;
                HistoricalMDPackEvent ev(r->guid);
                ev.ts_and_price_pack = prices;
                histroical_request.event_consumer->push(ev);
                return;
            }
        }

        std::map<std::chrono::milliseconds, OHLC> prices;
        const bool success = request_historical_klines(
                symbol.symbol_name,
                histroical_timerange,
                [&prices,
                 symbol](std::map<std::chrono::milliseconds, OHLC> && ts_and_ohlc_map) {
                    // other thread
                    prices.merge(ts_and_ohlc_map);
                });

        if (!success) {
            std::cout << "ERROR: Failed to request klines" << std::endl;
            m_status.push(WorkStatus::Crashed);
            return;
        }

        auto & range = m_ranges_by_symbol[symbol.symbol_name][histroical_timerange];
        HistoricalMDPackEvent ev(r->guid);
        ev.ts_and_price_pack = prices;
        histroical_request.event_consumer->push(ev);
        range.merge(prices);

        return;
    }
    if (const auto * r = std::get_if<LiveMDRequest>(&request); r) {
        handle_request(*r);
        return;
    }
    std::cout << "ERROR: Got unknown MD request" << std::endl;
}

void ByBitGateway::unsubscribe_from_live(xg::Guid guid)
{
    auto live_req_locked = m_live_requests.lock();
    for (auto it = live_req_locked.get().begin(), end = live_req_locked.get().end(); it != end; ++it) {
        if (it->guid == guid) {
            std::cout << "Erasing live request: " << guid << std::endl;
            ws_client.unsubscribe("publicTrade." + it->symbol.symbol_name);
            live_req_locked.get().erase(it);
            // if (live_req_locked.get().empty()) {
            //     m_live_thread->stop_async();
            // }
            break;
        }
    }
}
