#pragma once

#include "MarketOrder.h"
#include "Trade.h"

#include <optional>
#include <vector>

struct OrderRequestEvent;
class ITradingGateway
{
public:
    virtual ~ITradingGateway() = default;

    virtual std::optional<std::vector<Trade>> send_order_sync(const MarketOrder & order) = 0;
    virtual void push_order_request(const OrderRequestEvent & order) = 0;
};
