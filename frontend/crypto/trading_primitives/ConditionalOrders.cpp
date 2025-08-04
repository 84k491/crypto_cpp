#include "ConditionalOrders.h"

#include "crossguid/guid.hpp"

#include <utility>

void ConditionalMarketOrder::on_state_changed(bool active)
{
    m_suspended_volume = active ? m_target_volume : UnsignedVolume{};
}

void ConditionalMarketOrder::cancel()
{
    m_cancel_requested = true;
    m_target_volume = m_filled_volume;
}

OrderStatus ConditionalMarketOrder::status() const
{
    if (!m_reject_reason.empty()) {
        return OrderStatus::Rejected;
    }

    if (m_cancel_requested) {
        if (m_target_volume != m_filled_volume) {
            return OrderStatus::Pending;
        }

        return OrderStatus::Cancelled;
    }

    if (m_suspended_volume.value() > 0) {
        return OrderStatus::Suspended;
    }

    if (m_filled_volume < m_target_volume) {
        return OrderStatus::Pending;
    }

    if (m_target_volume <= m_filled_volume) {
        return OrderStatus::Filled;
    }

    return OrderStatus::Rejected;
}

void ConditionalMarketOrder::on_trade(UnsignedVolume vol, double price, double fee)
{
    MarketOrder::on_trade(vol, price, fee);
    m_suspended_volume -= vol;
}

TpslFullPos::TpslFullPos(
        std::string symbol,
        double take_profit_price,
        double stop_loss_price,
        Side side,
        std::chrono::milliseconds signal_ts)
    : m_symbol(std::move(symbol))
    , m_guid(xg::newGuid())
    , m_take_profit_price(take_profit_price)
    , m_stop_loss_price(stop_loss_price)
    , m_side(side)
    , m_signal_ts(signal_ts)
{
}

OrderStatus TpslFullPos::status() const
{
    if (m_reject_reason.has_value()) {
        return OrderStatus::Rejected;
    }

    if (m_triggered) {
        return OrderStatus::Filled;
    }

    if (m_set_up) {
        return OrderStatus::Suspended;
    }
    else {
        if (m_acked) {
            return OrderStatus::Cancelled;
        }
        else {
            return OrderStatus::Pending;
        }
    }
}

double TpslFullPos::take_profit_price() const
{
    return m_take_profit_price;
}

double TpslFullPos::stop_loss_price() const
{
    return m_stop_loss_price;
}

xg::Guid TpslFullPos::guid() const
{
    return m_guid;
}
