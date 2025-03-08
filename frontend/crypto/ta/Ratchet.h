#pragma once

#include <utility>

class Ratchet
{
public:
    Ratchet(double retracement);

    std::pair<double, bool> push_price_for_value(double price);

private:
    double calc_retracement_value(double price) const;
    int price_diff_sign(double price) const;

private:
    bool m_inited = false;
    bool m_first_flip_done = false;

    int m_current_trend_side = 0;

    double m_retracement = 0.;

    double m_last_value = 0.;
};
