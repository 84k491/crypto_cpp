#pragma once

struct OrderRequestEvent;
class ITradingGateway
{
public:
    virtual ~ITradingGateway() = default;

    virtual void push_order_request(const OrderRequestEvent & order) = 0;
};
