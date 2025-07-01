#include "ConditionalOrders.h"

void ConditionalMarketOrder::on_state_changed(bool active)
{
    m_suspended_volume = active ? m_target_volume : UnsignedVolume{};
}

OrderStatus ConditionalMarketOrder::status() const
{
    if (!m_reject_reason.empty()) {
        return OrderStatus::Rejected;
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
