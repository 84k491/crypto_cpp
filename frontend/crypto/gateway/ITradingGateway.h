#pragma once

struct OrderRequestEvent;
struct TpslRequestEvent;

class ITradingGateway
{
public:
    virtual ~ITradingGateway() = default;

    virtual void push_order_request(const OrderRequestEvent & order) = 0;
    virtual void push_tpsl_request(const TpslRequestEvent & tpsl_ev) = 0;
};
