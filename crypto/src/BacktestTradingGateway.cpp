#include "BacktestTradingGateway.h"

#include "ohlc.h"

BacktestTradingGateway::BacktestTradingGateway(Symbol symbol, ByBitGateway & md_gateway)
    : m_symbol(std::move(symbol))
    , m_md_gateway(md_gateway)
{
    std::cout << "Starting BacktestTradingGateway on: " << m_symbol.symbol_name << std::endl;
    m_klines_sub = m_md_gateway.subscribe_for_klines([this](std::chrono::milliseconds, const OHLC & ohlc) {
        m_last_price_by_symbol[m_symbol.symbol_name] = ohlc.close;
    });
}

std::optional<std::vector<Trade>> BacktestTradingGateway::send_order_sync(const MarketOrder & order)
{
    const auto price_it = m_last_price_by_symbol.find(order.symbol());
    if (price_it == m_last_price_by_symbol.end()) {
        std::cout << "ERROR: price not found for symbol " << m_symbol.symbol_name << std::endl;
        return std::nullopt;
    }
    const auto & price = price_it->second;
    const auto volume = order.volume();

    const double currency_spent = price * volume.value();
    const double fee = currency_spent * taker_fee_rate;
    Trade trade{
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()),
            m_symbol.symbol_name,
            price,
            volume,
            order.side(),
            fee,
    };

    return {{trade}};
}
