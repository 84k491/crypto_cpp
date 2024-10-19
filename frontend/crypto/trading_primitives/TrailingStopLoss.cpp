#include "TrailingStopLoss.h"

std::optional<StopLoss> TrailingStopLoss::calc_new_stop_loss(const double current_price, const std::optional<StopLoss> & previous_stop_loss) const
{
    const auto new_stop_price = current_price + m_price_distance * m_side.sign();
    if (!previous_stop_loss) {
        return {{m_symbol_name, new_stop_price, m_side}};
    }

    // For long, stop loss will be sell. In this case new price must be lower
    if (new_stop_price * m_side.sign() > previous_stop_loss->stop_price() * m_side.sign()) {
        return std::nullopt;
    }

    return {{m_symbol_name, new_stop_price, m_side}};
}
