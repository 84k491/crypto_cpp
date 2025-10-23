#pragma once

#include "Events.h"

#include <chrono>

class ILambdaAcceptor
{
public:
    virtual ~ILambdaAcceptor() = default;

    virtual void push(LambdaEvent value) = 0;
    virtual void push_delayed(std::chrono::milliseconds, LambdaEvent)
    {
        throw std::runtime_error("push_delayed not implemented");
    }
};
