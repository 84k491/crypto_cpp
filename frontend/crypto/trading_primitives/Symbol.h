#pragma once

#include "Volume.h"

#include <cmath>
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

    std::optional<SignedVolume> get_qty_floored(SignedVolume current_volume) const
    {
        if (lot_size_filter.qty_step == 0) {
            return std::nullopt;
        }
        const auto abs_volume_mul = std::abs(current_volume.value()) / lot_size_filter.qty_step;
        const auto floored_vol = std::floor(abs_volume_mul) * lot_size_filter.qty_step;
        const auto res = std::copysign(floored_vol, current_volume.value());
        return SignedVolume(res);
    }
};
