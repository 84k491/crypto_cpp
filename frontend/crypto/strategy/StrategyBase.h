#pragma once

#include "EventChannel.h"
#include "OrderManager.h"
#include "StrategyChannels.h"
#include "StrategyInterface.h"

class StrategyBase : public IStrategy
{
public:
    StrategyBase(OrderManager & order_manager,
                 EventLoopSubscriber & event_loop,
                 StrategyChannelsRefs channels);

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override
    {
        return m_strategy_internal_data_channel;
    }

    EventChannel<std::pair<std::string, bool>> & error_channel() override { return m_error_channel; }

protected:
    bool try_send_order(Side side, double price, std::chrono::milliseconds ts);

protected:
    const double m_pos_currency_amount = 100.;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;

    OrderManager & m_order_manager;
    bool m_has_opened_pos = false;

    EventChannel<std::pair<std::string, bool>> m_error_channel;

    std::list<std::shared_ptr<ISubscription>> m_channel_subs;
};
