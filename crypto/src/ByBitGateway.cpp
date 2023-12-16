#include "ByBitGateway.h"

#include <chrono>
#include <future>
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

void ByBitGateway::get_klines(const std::string & symbol, const Timerange & timerange, KlineCallback && kline_callback)
{
    if (auto it = m_ranges.find(timerange); it != m_ranges.end()) {
        for (const auto & [ts, ohlc] : it->second) {
            kline_callback({ts, ohlc});
        }
        return;
    }

    const bool success = request_klines(symbol, timerange, [this, timerange, kline_callback](std::map<std::chrono::milliseconds, OHLC> && ts_and_ohlc_map) {
        auto & range = m_ranges[timerange];
        for (const auto & ts_and_ohlc : ts_and_ohlc_map) {
            kline_callback(ts_and_ohlc);
        }
        range.merge(ts_and_ohlc_map);
    });
    if (!success) {
        std::cout << "ERROR: Failed to request klines" << std::endl;
    }
}

bool ByBitGateway::request_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && pack_callback)
{
    std::cout << "Whole requested interval hours: " << std::chrono::duration_cast<std::chrono::hours>(timerange.duration()).count() << std::endl;

    const auto min_interval = std::chrono::minutes{1};
    auto last_ts = timerange.start();
    while (last_ts < timerange.end()) {
        auto future = std::async(
                std::launch::async,
                [&symbol, this, timerange, min_interval, start = last_ts]() {
                    std::condition_variable cv;
                    std::mutex m;

                    const std::string category = "linear";
                    const unsigned limit = 1000;

                    const std::string url = [&]() {
                        std::stringstream ss;
                        ss << "https://api-testnet.bybit.com/v5/market/kline"
                           << "?symbol=" << symbol
                           << "&category=" << category
                           << "&interval=" << min_interval.count()
                           << "&limit=" << limit
                           << "&start=" << start.count();
                        return std::string(ss.str());
                    }();

                    std::cout << "REST request: " << url << std::endl;
                    std::string string_result;
                    client
                            .Build()
                            ->Get(url)
                            .WithCompletion([&](const restincurl::Result & result) {
                                std::lock_guard lock(m);
                                string_result = result.body;
                                cv.notify_all();
                            })
                            .Execute();

                    std::unique_lock lock(m);
                    cv.wait(lock, [&] { return !string_result.empty(); });

                    OhlcResponse response;
                    const auto j = json::parse(string_result);
                    from_json(j, response);

                    std::map<std::chrono::milliseconds, OHLC> furure_result;
                    for (const auto & ohlc : response.result.ohlc_list) {
                        if (timerange.contains(ohlc.timestamp)) {
                            furure_result.try_emplace(ohlc.timestamp, ohlc);
                        }
                    }

                    return furure_result;
                });
        future.wait();
        std::map<std::chrono::milliseconds, OHLC> inter_result = future.get();
        std::cout << "Got " << inter_result.size() << " prices" << std::endl;
        if (!inter_result.empty()) {
            if (const auto delta = inter_result.begin()->first - last_ts; delta > min_interval) {
                std::cout << "ERROR inconsistent time delta: " << std::chrono::duration_cast<std::chrono::seconds>(delta).count() << "s. Stopping" << std::endl;
                std::cout << "First timestamp: " << inter_result.begin()->first.count() << std::endl;
                return false;
            }
            last_ts = inter_result.rbegin()->first;
        }
        pack_callback(std::move(inter_result));
    }
    const auto received_interval = last_ts - timerange.start();
    std::cout << "Whole received interval hours: " << std::chrono::duration_cast<std::chrono::hours>(received_interval).count() << std::endl;
    std::cout << timerange.start().count() << "-" << last_ts.count() << std::endl;

    return true;
}
