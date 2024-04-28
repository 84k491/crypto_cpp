#include "Events.h"

HistoricalMDRequest::HistoricalMDRequest(IEventConsumer<HistoricalMDPackEvent> & consumer,
                                         const Symbol & symbol,
                                         HistoricalMDRequestData data)
    : BasicEvent<HistoricalMDPackEvent>(consumer)
    , data(data)
    , symbol(symbol)
    , guid(xg::newGuid())
{
}

LiveMDRequest::LiveMDRequest(IEventConsumer<MDPriceEvent> & consumer, const Symbol & symbol)
    : BasicEvent<MDPriceEvent>(consumer)
    , symbol(symbol)
    , guid(xg::newGuid())
{
}

HistoricalMDPackEvent::HistoricalMDPackEvent(xg::Guid request_guid)
    : request_guid(request_guid)
{
}
