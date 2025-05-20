#pragma once

#include "StrategyInterface.h"

class StrategyBase : public IStrategy
{
public:
    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override
    {
        return m_strategy_internal_data_channel;
    }

    EventTimeseriesChannel<Signal> & signal_channel() override
    {
        return m_signal_channel;
    }

protected:
    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
    EventTimeseriesChannel<Signal> m_signal_channel;

    std::list<std::shared_ptr<ISubscription>> m_channel_subs;
};
