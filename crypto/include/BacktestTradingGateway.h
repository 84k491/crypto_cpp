#pragma once

#include "ByBitGateway.h"
#include "ITradingGateway.h"
#include "Symbol.h"

#include <unordered_map>

class BacktestTradingGateway : public ITradingGateway
{
public:
    static constexpr double taker_fee_rate = 0.00055;
    BacktestTradingGateway(Symbol symbol, ByBitGateway & md_gateway);

    std::optional<std::vector<Trade>> send_order_sync(const MarketOrder & order) override;

private:
    std::unordered_map<std::string, double> m_last_price_by_symbol;

    const Symbol m_symbol;
    ByBitGateway & m_md_gateway;

    std::shared_ptr<ISubsription> m_klines_sub;
};
