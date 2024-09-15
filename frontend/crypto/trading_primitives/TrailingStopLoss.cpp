#include "TrailingStopLoss.h"

std::optional<StopLoss> TrailingStopLoss::calc_new_stop_loss(const double current_price, const std::optional<StopLoss> & previous_stop_loss) const
{
    const auto side_sign = m_side == Side::Buy ? 1 : -1;
    const auto new_stop_price = current_price + m_price_distance * side_sign;
    if (!previous_stop_loss) {
        return {{m_symbol, new_stop_price, m_side}};
    }

    // For long, stop loss will be sell. In this case new price must be lower
    if (new_stop_price * side_sign > previous_stop_loss->stop_price() * side_sign) {
        return std::nullopt;
    }

    return {{m_symbol, new_stop_price, m_side}};
}
