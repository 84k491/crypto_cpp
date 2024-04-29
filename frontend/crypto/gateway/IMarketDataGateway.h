#pragma once

#include "Events.h"
#include "ObjectPublisher.h"
#include "WorkStatus.h"

class IMarketDataGateway
{
public:
    virtual ~IMarketDataGateway() = default;

    virtual void push_async_request(HistoricalMDRequest && request) = 0;
    virtual void push_async_request(LiveMDRequest && request) = 0;
    virtual void unsubscribe_from_live(xg::Guid guid) = 0;

    virtual ObjectPublisher<WorkStatus> & status_publisher() = 0;
};
