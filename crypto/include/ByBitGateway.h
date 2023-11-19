#pragma once

#include "ohlc.h"
#include "restincurl.h"

#include <future>

class ByBitGateway
{
public:
    ByBitGateway() = default;

    std::future<std::vector<OHLC>> get_klines();

private:
    restincurl::Client client;
};
