#include "StrategyBase.h"

#include "Logger.h"

StrategyBase::StrategyBase(
        OrderManager & order_manager,
        EventLoop & event_loop,
        StrategyChannelsRefs channels)
    : m_order_manager(order_manager)
    , m_sub(event_loop)
{
    m_sub.subscribe(
            channels.opened_pos_channel,
            [this](const bool & v) {
                m_has_opened_pos = v;
            });
}

bool StrategyBase::try_send_order(Side side, double price, std::chrono::milliseconds ts)
{
    if (!m_order_manager.pending_orders().empty()) {
        return false;
    }

    if (m_has_opened_pos) {
        return false;
    }

    const auto default_pos_size_opt = UnsignedVolume::from(m_pos_currency_amount / price);
    if (!default_pos_size_opt.has_value()) {
        LOG_ERROR("can't get proper default position size");
        return false;
    }

    // TODO
    m_order_manager.send_market_order(
            price,
            SignedVolume{*default_pos_size_opt, side},
            ts);
    return true;
}
