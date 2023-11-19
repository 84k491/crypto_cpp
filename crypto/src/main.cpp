#include "restincurl.h"

#include <nlohmann/json.hpp>

#include <condition_variable>
#include <future>
#include <iostream>
#include <list>
#include <mutex>

struct OHLC
{
    OHLC(const std::string & timestamp,
         const std::string & open,
         const std::string & high,
         const std::string & low,
         const std::string & close,
         const std::string & volume,
         const std::string & turnover)
        : timestamp(std::stoull(timestamp))
        , open(std::stod(open))
        , high(std::stod(high))
        , low(std::stod(low))
        , close(std::stod(close))
        , volume(std::stod(volume))
        , turnover(std::stod(turnover))
    {
    }
    uint64_t timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
    double turnover;
};

using json = nlohmann::json;

void to_json(json & j, const OHLC & ohlc)
{
    j = json{{"timestamp", ohlc.timestamp},
             {"open", ohlc.open},
             {"high", ohlc.high},
             {"low", ohlc.low},
             {"close", ohlc.close},
             {"volume", ohlc.volume},
             {"turnover", ohlc.turnover}};
}

std::ostream & operator<<(std::ostream & os, const OHLC & ohlc)
{
    const auto j = json(ohlc);
    os << "OHLC\n"
       << std::setw(4) << j;
    return os;
}

struct OhlcResult
{
    std::string category;
    std::string symbol;
    std::list<OHLC> ohlc_list;
};

void from_json(const json & j, OhlcResult & result)
{
    j.at("category").get_to(result.category);
    j.at("symbol").get_to(result.symbol);

    result.ohlc_list.clear();
    for (const auto & ohlc_list : j.at("list")) {
        OHLC ohlc(ohlc_list[0].get<std::string>(),
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

class ByBitGateway
{
public:
    ByBitGateway() = default;

    std::future<std::string> get_klines()
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
            return string_result;
        });
    }

private:
    restincurl::Client client;
};

int main(int argc, char * argv[])
{
    ByBitGateway gateway;
    auto future = gateway.get_klines();
    future.wait();
    OhlcResponse response;
    json j = json::parse(future.get());
    from_json(j, response);
    for (auto ohlc : response.result.ohlc_list) {
        std::cout << ohlc << std::endl;
    }
    return 0;
}
