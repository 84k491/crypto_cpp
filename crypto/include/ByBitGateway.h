#pragma once

#include "Symbol.h"
#include "Timerange.h"
#include "ohlc.h"
#include "restincurl.h"

#include <functional>
#include <unordered_map>
#include <vector>

class ByBitGateway
{
    static constexpr double taker_fee = 0.0002; // 0.02%

public:
    using KlineCallback = std::function<void(std::pair<std::chrono::milliseconds, OHLC>)>;
    using KlinePackCallback = std::function<void(std::map<std::chrono::milliseconds, OHLC> &&)>;

    static constexpr std::chrono::minutes min_interval = std::chrono::minutes{1};

    ByBitGateway() = default;

    bool get_klines(const std::string & symbol, const Timerange & timerange, KlineCallback && cb);
    static auto get_taker_fee() { return taker_fee; }

    std::vector<Symbol> get_symbols(const std::string & currency);

private:
    void merge_intersecting_ranges();
    bool request_klines(const std::string & symbol, const Timerange & timerange, KlinePackCallback && cb);

    std::unordered_map<std::string, std::unordered_map<Timerange, std::map<std::chrono::milliseconds, OHLC>>> m_ranges_by_symbol;
    restincurl::Client client; // TODO use mrtazz/restclient-cpp
};
