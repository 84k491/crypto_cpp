#include "ByBitGateway.h"

#include "ScopeExit.h"
#include "ohlc.h"

#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

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

void from_json(const json & j, Symbol & symbol)
{
    j.at("symbol").get_to(symbol.symbol_name);
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

bool ByBitGateway::get_klines(const std::string & symbol,
                              KlineCallback && kline_callback,
                              const std::optional<std::chrono::milliseconds> & start,
                              const std::optional<std::chrono::milliseconds> & end)
{
    ScopeExit se([&] { m_status.push(WorkStatus::Crashed); });
    m_status.push(WorkStatus::Backtesting);

    if (m_last_server_time == std::chrono::milliseconds{0}) {
        m_last_server_time = get_server_time() - min_interval;
        std::cout << "Modified server time: " << m_last_server_time.count() << std::endl;
    }

    if (start.has_value()) {
        if (start.value() > m_last_server_time) {
            std::cout << "ERROR starting in future" << std::endl;
            return false;
        }

        const auto historical_end = end.has_value() ? std::min(end.value(), m_last_server_time) : m_last_server_time;
        const auto histroical_timerange = Timerange{start.value(), historical_end};

        bool cached = false;
        if (auto range_it = m_ranges_by_symbol.find(symbol); range_it != m_ranges_by_symbol.end()) {
            if (auto it = range_it->second.find(histroical_timerange); it != range_it->second.end()) {
                for (const auto & [ts, ohlc] : it->second) {
                    kline_callback({ts, ohlc});
                }
                cached = true;
            }
        }

        const bool success = cached ||
                request_historical_klines(
                                     symbol,
                                     histroical_timerange,
                                     [this,
                                      symbol,
                                      histroical_timerange,
                                      kline_callback](std::map<std::chrono::milliseconds, OHLC> && ts_and_ohlc_map) {
                                         auto & range = m_ranges_by_symbol[symbol][histroical_timerange];
                                         for (const auto & ts_and_ohlc : ts_and_ohlc_map) {
                                             kline_callback(ts_and_ohlc);
                                         }
                                         range.merge(ts_and_ohlc_map);
                                     });
        if (!success) {
            std::cout << "ERROR: Failed to request klines" << std::endl;
            return false;
        }
    }

    if (end.has_value() && end.value() < m_last_server_time) {
        std::cout << "Don't continue live" << std::endl;
        m_status.push(WorkStatus::Stopped);
        return true;
    }

    std::cout << "Going live" << std::endl;
    m_status.push(WorkStatus::Live);
    m_running.store(true);

    auto last_ts = m_last_server_time;
    for (;;) {
        const auto timerange = Timerange{last_ts + std::chrono::seconds{1}, last_ts + min_interval};

        request_historical_klines(
                symbol,
                timerange,
                [symbol,
                 timerange,
                 kline_callback,
                 &last_ts](std::map<std::chrono::milliseconds, OHLC> && ts_and_ohlc_map) {
                    for (const auto & ts_and_ohlc : ts_and_ohlc_map) {
                        kline_callback(ts_and_ohlc);
                        last_ts = ts_and_ohlc.first;
                    }
                });

        if (!m_running.load() || (end.has_value() && last_ts > end.value())) {
            m_status.push(WorkStatus::Stopped);
            std::cout << "Stopping gateway" << std::endl;
            return true;
        }
        else {
            const auto wait_time = std::chrono::seconds{30};
            std::cout << "Got all live prices, sleeping for " << wait_time.count() << " seconds" << std::endl;
            const auto now = std::chrono::steady_clock::now();
            while (m_running && std::chrono::steady_clock::now() < now + wait_time) {
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        }
    }

    std::cout << "ERROR unreachable code" << std::endl;
    return false;
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

        const std::chrono::milliseconds remining_delta = timerange.end() - last_start;
        std::cout << "Remaining time delta: " << remining_delta.count() << "ms" << std::endl;

        const std::string category = "spot";

        const std::string request = [&]() {
            std::stringstream ss;
            ss << "https://api-testnet.bybit.com/v5/market/kline"
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
        if (!inter_result.empty()) {
            if (const auto delta = inter_result.begin()->first - last_start; delta > min_interval) {
                std::cout << "ERROR inconsistent time delta: " << std::chrono::duration_cast<std::chrono::seconds>(delta).count() << "s. Stopping" << std::endl;
                std::cout << "First timestamp: " << inter_result.begin()->first.count() << std::endl;
                return false;
            }
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
        ss << "https://api-testnet.bybit.com/v5/market/instruments-info"
           << "?category=" << category
           << "&limit=" << limit;
        return std::string(ss.str());
    }();

    auto str_future = rest_client.request_async(url);
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

std::future<std::string> RestClient::request_async(const std::string & request)
{
    return std::async(
            std::launch::async,
            [this, &request]() {
                std::condition_variable cv;
                std::mutex m;

                std::cout << "REST request: " << request << std::endl;
                std::string string_result;
                client
                        .Build()
                        ->Get(request)
                        .WithCompletion([&](const restincurl::Result & result) {
                            std::lock_guard lock(m);
                            string_result = result.body;
                            cv.notify_all();
                        })
                        .Execute();

                std::unique_lock lock(m);
                cv.wait(lock, [&] { return !string_result.empty(); });

                return string_result;
            });
}

std::chrono::milliseconds ByBitGateway::get_server_time()
{
    const std::string url = "https://api-testnet.bybit.com/v5/market/time";

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

void ByBitGateway::stop()
{
    m_running = false;
}
