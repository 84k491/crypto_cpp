#include "ByBitGateway.h"

#include <condition_variable>
#include <list>
#include <mutex>
#include <vector>

struct OhlcResult
{
    std::string category;
    std::string symbol;
    std::list<OHLC> ohlc_list;
};

OHLC from_strings(const std::string & timestamp,
                  const std::string & open,
                  const std::string & high,
                  const std::string & low,
                  const std::string & close,
                  const std::string & volume,
                  const std::string & turnover)
{
    return OHLC{std::stoull(timestamp),
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

std::future<std::vector<OHLC>> ByBitGateway::get_klines()
{
    return std::async(std::launch::async, [&] {
        std::condition_variable cv;
        std::mutex m;
        std::string string_result;

        client
                .Build()
                ->Get("https://api-testnet.bybit.com/v5/market/kline?symbol=BTCUSDT&category=spot&interval=15")
                .WithCompletion([&](const restincurl::Result & result) {
                    std::lock_guard lock(m);
                    string_result = result.body;
                    cv.notify_all();
                })
                .Execute();

        std::unique_lock lock(m);
        cv.wait(lock, [&] { return !string_result.empty(); });
        std::vector<OHLC> result;
        OhlcResponse response;
        const auto j = json::parse(string_result);
        from_json(j, response);
        for (const auto & ohlc : response.result.ohlc_list) {
            result.push_back(ohlc);
        }
        return result;
    });
}
