#include "Events.h"
#include "BybitTradesDownloader.h"

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

std::optional<HistoricalMDPriceEvent> HistoricalMDGeneratorEvent::get_next()
{
    const auto price_opt = m_reader->get_next();
    if (!price_opt.has_value()) {
        return std::nullopt;
    }
    return HistoricalMDPriceEvent{price_opt.value()};
}
