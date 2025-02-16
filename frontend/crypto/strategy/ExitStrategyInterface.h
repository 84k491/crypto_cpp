#pragma once

#include "EventTimeseriesChannel.h"
#include "Events.h"
#include "Position.h"
#include "Trade.h"

#include <chrono>
#include <utility>

class IExitStrategy
{
public:
    virtual ~IExitStrategy() = default;

    virtual EventTimeseriesChannel<Tpsl> & tpsl_channel() = 0;
    virtual EventTimeseriesChannel<StopLoss> & trailing_stop_channel() = 0;

    [[nodiscard]] virtual std::optional<std::string> on_price_changed(std::pair<std::chrono::milliseconds, double> ts_and_price) = 0;
    [[nodiscard]] virtual std::optional<std::string> on_trade(const std::optional<OpenedPosition> & opened_position, const Trade & trade) = 0;

    [[nodiscard]] virtual std::optional<std::pair<std::string, bool>>
    handle_event(const TpslResponseEvent & response) = 0;

    [[nodiscard]] virtual std::optional<std::pair<std::string, bool>>
    handle_event(const TpslUpdatedEvent & response) = 0;

    [[nodiscard]] virtual std::optional<std::pair<std::string, bool>>
    handle_event(const TrailingStopLossResponseEvent & response) = 0;

    [[nodiscard]] virtual std::optional<std::pair<std::string, bool>>
    handle_event(const TrailingStopLossUpdatedEvent & response) = 0;

    virtual std::optional<std::pair<std::string, bool>>
    push_price_level(const ProfitPriceLevels & levels) = 0;
};
