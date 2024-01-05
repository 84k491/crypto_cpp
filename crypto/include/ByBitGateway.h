#pragma once

#include "Symbol.h"
#include "Timerange.h"
#include "ohlc.h"
#include "restincurl.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <unordered_map>
#include <vector>

class RestClient
{
public:
    std::future<std::string> request_async(const std::string & request);

private:
    restincurl::Client client; // TODO use mrtazz/restclient-cpp
};

class ByBitGateway
{
    static constexpr double taker_fee = 0.0002; // 0.02%

public:
    using KlineCallback = std::function<void(std::pair<std::chrono::milliseconds, OHLC>)>;

    static constexpr std::chrono::minutes min_interval = std::chrono::minutes{1};

    ByBitGateway() = default;

    bool get_klines(
            const std::string & symbol,
            KlineCallback && cb,
            const std::optional<std::chrono::milliseconds> & start,
            const std::optional<std::chrono::milliseconds> & end);

    static auto get_taker_fee() { return taker_fee; }

    std::vector<Symbol> get_symbols(const std::string & currency);

private:
    using KlinePackCallback = std::function<void(std::map<std::chrono::milliseconds, OHLC> &&)>;

    std::chrono::milliseconds get_server_time();
    bool request_historical_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && cb);

private:
    std::chrono::milliseconds m_last_server_time = std::chrono::milliseconds{0};

    std::unordered_map<std::string, std::unordered_map<Timerange, std::map<std::chrono::milliseconds, OHLC>>> m_ranges_by_symbol;
    restincurl::Client client; // TODO use mrtazz/restclient-cpp
    RestClient rest_client;
};
