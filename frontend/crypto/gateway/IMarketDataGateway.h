#pragma once

#include "EventObjectChannel.h"
#include "EventChannel.h"
#include "Events.h"
#include "WorkStatus.h"

class IMarketDataGateway
{
public:
    virtual ~IMarketDataGateway() = default;

    virtual void push_async_request(HistoricalMDRequest && request) = 0;
    virtual void push_async_request(LiveMDRequest && request) = 0;

    virtual EventChannel<HistoricalMDGeneratorEvent> & historical_prices_channel() = 0;
    virtual EventChannel<HistoricalMDGeneratorLowMemEvent> & historical_lowmem_channel() = 0;
    virtual EventChannel<MDPriceEvent> & live_prices_channel() = 0;

    virtual void unsubscribe_from_live(xg::Guid guid) = 0;

    virtual EventObjectChannel<WorkStatus> & status_channel() = 0;
};
