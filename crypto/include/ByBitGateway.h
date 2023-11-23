#pragma once

#include "ohlc.h"
#include "restincurl.h"

#include <future>
#include <optional>

class ByBitGateway
{
public:
    ByBitGateway() = default;

    std::future<std::map<std::chrono::milliseconds, OHLC>> get_klines(
            std::chrono::milliseconds start,
            std::optional<std::chrono::milliseconds> end);

private:
    restincurl::Client client;
};
