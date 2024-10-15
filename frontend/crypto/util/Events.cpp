#include "Events.h"

HistoricalMDRequest::HistoricalMDRequest(const Symbol & symbol,
                                         HistoricalMDRequestData data)
    : data(data)
    , symbol(symbol)
    , guid(xg::newGuid())
{
}

LiveMDRequest::LiveMDRequest(
        const std::shared_ptr<IEventConsumer<MDPriceEvent>> & consumer,
        const Symbol & symbol)
    : EventWithResponse<MDPriceEvent>(consumer)
    , symbol(symbol)
    , guid(xg::newGuid())
{
}

HistoricalMDPackEvent::HistoricalMDPackEvent(xg::Guid request_guid)
    : request_guid(request_guid)
{
}

std::ostream & operator<<(std::ostream & os, const HistoricalMDRequestData & data)
{
    return os << "HistoricalMDRequestData{"
              << "start: " << data.start << ", end: " << data.end << "}";
}
