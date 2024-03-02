#include "BacktestTradingGateway.h"

BacktestTradingGateway::BacktestTradingGateway() = default;

std::optional<std::vector<Trade>> BacktestTradingGateway::send_order_sync(const MarketOrder & order)
{
    const auto & price = order.price();
    const auto volume = order.volume();

    const double currency_spent = price * volume.value();
    const double fee = currency_spent * taker_fee_rate;
    Trade trade{
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()),
            order.symbol(),
            price,
            volume,
            order.side(),
            fee,
    };

    return {{trade}};
}
