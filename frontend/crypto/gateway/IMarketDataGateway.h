#pragma once

#include "EventObjectPublisher.h"
#include "EventPublisher.h"
#include "Events.h"
#include "WorkStatus.h"

class IMarketDataGateway
{
public:
    virtual ~IMarketDataGateway() = default;

    virtual void push_async_request(HistoricalMDRequest && request) = 0;
    virtual void push_async_request(LiveMDRequest && request) = 0;

    virtual EventPublisher<HistoricalMDPackEvent> & historical_prices_publisher() = 0;
    virtual EventPublisher<MDPriceEvent> & live_prices_publisher() = 0;

    virtual void unsubscribe_from_live(xg::Guid guid) = 0;

    virtual EventObjectPublisher<WorkStatus> & status_publisher() = 0;
};
