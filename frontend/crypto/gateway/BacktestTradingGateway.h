#pragma once

#include "ITradingGateway.h"

class BacktestTradingGateway : public ITradingGateway
{
public:
    static constexpr double taker_fee_rate = 0.00055;
    BacktestTradingGateway();

    std::optional<std::vector<Trade>> send_order_sync(const MarketOrder & order) override;
};
