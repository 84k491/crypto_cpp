#pragma once

#include "Timerange.h"
#include "ohlc.h"
#include "restincurl.h"

#include <unordered_map>
#include <vector>

class ByBitGateway
{
public:
    static constexpr std::chrono::minutes min_interval = std::chrono::minutes{1};

    ByBitGateway() = default;

    void receive_klines(const Timerange & timerange);
    void subscribe_for_klines(std::function<void(std::pair<std::chrono::milliseconds, OHLC>)> on_kline_received_cb);

private:
    void merge_intersecting_ranges();
    std::map<std::chrono::milliseconds, OHLC> request_klines(const Timerange &timerange);

    std::unordered_map<Timerange, std::map<std::chrono::milliseconds, OHLC>> m_ranges;

    std::vector<std::function<void(std::pair<std::chrono::milliseconds, OHLC>)>> m_kline_callbacks;
    restincurl::Client client;
};
