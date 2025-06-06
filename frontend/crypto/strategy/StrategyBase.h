#pragma once

#include "OrderManager.h"
#include "StrategyInterface.h"

class StrategyBase : public IStrategy
{
public:
    StrategyBase(OrderManager & order_manager);

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override
    {
        return m_strategy_internal_data_channel;
    }

protected:
    bool try_send_order(Side side, double price, std::chrono::milliseconds ts, OrderManager::OrderCallback && on_response);
    bool send_order(Side side, double price, std::chrono::milliseconds ts, OrderManager::OrderCallback && on_response);

protected:
    const double m_pos_currency_amount = 100.;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;

    std::list<std::shared_ptr<ISubscription>> m_channel_subs;

    OrderManager & m_order_manager;
};
