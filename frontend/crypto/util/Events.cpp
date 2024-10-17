#include "Events.h"

HistoricalMDRequest::HistoricalMDRequest(const Symbol & symbol,
                                         HistoricalMDRequestData data)
    : data(data)
    , symbol(symbol)
    , guid(xg::newGuid())
{
}

LiveMDRequest::LiveMDRequest(
        const Symbol & symbol)
    : symbol(symbol)
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
