#include "Ratchet.h"

Ratchet::Ratchet(double retracement)
    : m_retracement(retracement)
{
}

double Ratchet::calc_retracement_value(double price) const
{
    const auto delta = price * m_retracement;
    return price - (delta * m_current_trend_side);
}

int Ratchet::price_diff_sign(double price) const
{
    return price > m_last_value ? 1 : -1;
}

std::pair<double, bool> Ratchet::push_price_for_value(double price)
{
    if (!m_inited) {
        m_current_trend_side = 1;
        m_inited = true;
        m_last_value = calc_retracement_value(price);
        return {m_last_value, false};
    }

    if (price_diff_sign(price) != m_current_trend_side) {
        m_current_trend_side = price_diff_sign(price);
        m_last_value = calc_retracement_value(price);

        if (!m_first_flip_done) {
            m_first_flip_done = true;
            return {m_last_value, false};
        }

        return {m_last_value, true};
    }

    const auto new_retracement = calc_retracement_value(price);
    if (price_diff_sign(new_retracement) != m_current_trend_side) {
        return {m_last_value, false};
    }

    m_last_value = new_retracement;
    return {m_last_value, false};
}
