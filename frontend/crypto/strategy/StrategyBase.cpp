#include "StrategyBase.h"

#include "Logger.h"

StrategyBase::StrategyBase(OrderManager & order_manager)
    : m_order_manager(order_manager)
{
}

bool StrategyBase::try_send_order(Side side, double price, std::chrono::milliseconds ts)
{
    if (!m_order_manager.pending_orders().empty()) {
        return false;
    }

    const auto default_pos_size_opt = UnsignedVolume::from(m_pos_currency_amount / price);
    if (!default_pos_size_opt.has_value()) {
        Logger::log<LogLevel::Error>("can't get proper default position size");
        return false;
    }

    // TODO
    m_order_manager.send_market_order(
            price,
            SignedVolume{*default_pos_size_opt, side},
            ts);
    return true;
}
