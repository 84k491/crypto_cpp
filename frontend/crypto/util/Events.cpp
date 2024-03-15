#include "Events.h"

HistoricalMDRequest::HistoricalMDRequest(IEventConsumer<HistoricalMDPackEvent> & _consumer, const Symbol & _symbol, std::chrono::milliseconds _start, std::chrono::milliseconds _end)
    : BasicEvent<HistoricalMDPackEvent>(_consumer)
    , symbol(_symbol)
    , start(_start)
    , end(_end)
    , guid(xg::newGuid())
{
}

LiveMDRequest::LiveMDRequest(IEventConsumer<MDPriceEvent> & _consumer, const Symbol & _symbol)
    : BasicEvent<MDPriceEvent>(_consumer)
    , symbol(_symbol)
    , guid(xg::newGuid())
{
}

HistoricalMDPackEvent::HistoricalMDPackEvent(xg::Guid request_guid)
    : request_guid(request_guid)
{
}
