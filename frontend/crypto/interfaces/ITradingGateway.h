#pragma once

#include "TradingPrimitives.h"

#include <optional>
#include <vector>

class ITradingGateway
{
public:
    virtual ~ITradingGateway() = default;

    virtual std::optional<std::vector<Trade>> send_order_sync(const MarketOrder & order) = 0;
};
