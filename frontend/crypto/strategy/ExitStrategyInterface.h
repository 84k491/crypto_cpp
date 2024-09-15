#pragma once

#include "EventTimeseriesPublisher.h"
#include "Events.h"
#include "Position.h"
#include "Trade.h"

#include <chrono>
#include <utility>

class IExitStrategy
{
public:
    virtual ~IExitStrategy() = default;

    virtual EventTimeseriesPublisher<Tpsl> & tpsl_publisher() = 0;
    virtual EventTimeseriesPublisher<StopLoss> & trailing_stop_publisher()  = 0;

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
};
