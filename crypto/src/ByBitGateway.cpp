#include "ByBitGateway.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
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

std::future<std::map<std::chrono::milliseconds, OHLC>> ByBitGateway::get_klines(std::chrono::milliseconds start,
                                                                                std::optional<std::chrono::milliseconds> end)
{
    return std::async(std::launch::async, [this, start, end]() {
        std::condition_variable cv;
        std::mutex m;

        const std::string symbol = "BTCUSDT";
        const std::string category = "spot";
        const unsigned interval = 15;

        const std::string url = [&]() {
            std::stringstream ss;
            ss << "https://api-testnet.bybit.com/v5/market/kline"
               << "?symbol=" << symbol
               << "&category=" << category
               << "&interval=" << interval
               << "&start=" << start.count();
            if (end.has_value()) {
                ss << "&end=" << end.value().count();
            }
            return std::string(ss.str());
        }();

        long result_code = -1;

        std::cout << "REST request: " << url << std::endl;
        std::string string_result;
        client
                .Build()
                ->Get(url)
                .WithCompletion([&](const restincurl::Result & result) {
                    std::lock_guard lock(m);
                    result_code = result.http_response_code;
                    string_result = result.body;
                    cv.notify_all();
                })
                .Execute();

        std::unique_lock lock(m);
        cv.wait(lock, [&] { return !string_result.empty(); });
        std::cout << "Result code: " << result_code << std::endl;

        OhlcResponse response;
        const auto j = json::parse(string_result);
        from_json(j, response);

        std::map<std::chrono::milliseconds, OHLC> result;
        for (const auto & ohlc : response.result.ohlc_list) {
            result.try_emplace(ohlc.timestamp, ohlc);
        }
        return result;
    });
}
