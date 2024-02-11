#pragma once

#include <cmath>
#include <iostream>
#include <optional>
#include <string>

struct Symbol
{
    struct LotSizeFilter
    {
        double min_qty = {};
        double max_qty = {};
        double qty_step = {};
    };
    std::string symbol_name;
    LotSizeFilter lot_size_filter;

    std::optional<double> get_qty_floored(double current_volume) const
    {
        const auto abs_volume_mul = std::abs(current_volume) / lot_size_filter.qty_step;
        const auto floored_vol = std::floor(abs_volume_mul) * lot_size_filter.qty_step;
        const auto res = std::copysign(floored_vol, current_volume);
        return res;
    }
};
