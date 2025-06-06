#pragma once

#include "EventLoopSubscriber.h"
#include "ITradingGateway.h"
#include "MarketOrder.h"
#include "Symbol.h"
#include "crossguid/guid.hpp"

#include <chrono>
#include <map>

class OrderManager
{
public:
    using OrderCallback = std::function<void(const MarketOrder&, bool)>;

    OrderManager(
            Symbol symbol,
            EventLoopSubscriber & event_loop,
            ITradingGateway & tr_gateway);

    void send_market_order(double price, SignedVolume vol, std::chrono::milliseconds ts, OrderCallback && on_response);
    const std::map<xg::Guid, std::pair<MarketOrder, OrderCallback>> & pending_orders() const;

private:
    void on_order_response(const OrderResponseEvent & r);

private:
    Symbol m_symbol;
    ITradingGateway & m_tr_gateway;

    std::map<xg::Guid, std::pair<MarketOrder, OrderCallback>> m_pending_orders;

    EventLoopSubscriber & m_event_loop;
    EventChannel<std::string> m_error_channel;
};
