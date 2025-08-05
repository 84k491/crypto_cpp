#pragma once

#include "Candle.h"
#include "TimeWeightedMovingAverage.h"

#include <optional>

class AverageDirectionalIndex
{
    using MA = TimeWeightedMovingAverage;

    struct DirectionalMovement
    {
        double positive = 0.;
        double negative = 0.;
    };

public:
    enum class Direction : uint8_t
    {
        UpTrend,
        DownTrend,
    };

    struct Result
    {
        double adx = 0.;
        Direction trend = Direction::UpTrend;
    };

    AverageDirectionalIndex(std::chrono::milliseconds interval);

    std::optional<Result> push_candle(const Candle & current_candle);

private:
    DirectionalMovement directional_movement(const Candle & current_candle);
    double true_range(const Candle & current_candle);

    std::optional<Candle> m_previous_candle;

    MA m_average_true_range;
    MA m_smoothed_pdm;
    MA m_smoothed_ndm;

    MA m_average_directional_index;
};
