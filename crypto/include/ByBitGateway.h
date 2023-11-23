#pragma once

#include "ohlc.h"
#include "restincurl.h"

#include <future>

class ByBitGateway
{
public:
    ByBitGateway() = default;

    void get_klines(std::chrono::milliseconds start, std::chrono::milliseconds end);
    void set_on_kline_received(std::function<void(std::pair<std::chrono::milliseconds, OHLC>)> on_kline_received_cb);

private:
    std::function<void(std::pair<std::chrono::milliseconds, OHLC>)> m_on_kline_received_cb;

    restincurl::Client client;
};
