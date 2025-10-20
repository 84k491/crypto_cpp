#pragma once

#include "EventLoop.h"
#include "EventLoopSubscriber.h"
#include "Scheduler.h"

#include <utility>

class IDelayedLambdaAcceptor : public ILambdaAcceptor
{
public:
    virtual ~IDelayedLambdaAcceptor() = default;

    virtual bool push_to_queue(LambdaEvent value) = 0;
    virtual bool push_to_queue_delayed(std::chrono::milliseconds delay, LambdaEvent value) = 0;
};

class EventLoopWithDelays : public ILambdaAcceptor
{
public:
    EventLoopWithDelays()
        : m_sub(m_ev)
    {
        auto & ch = Scheduler::i().delayed_channel(m_guid);
        m_sub.subscribe(
                ch,
                [this](LambdaEvent ev) {
                    m_ev.push(std::move(ev));
                });
    }

    void push(LambdaEvent event) override
    {
        return m_ev.push(std::move(event));
    }

    void push_delayed(std::chrono::milliseconds delay, LambdaEvent value) override
    {
        Scheduler::i().delay_event(m_guid, delay, value);
    }

private:
    xg::Guid m_guid;

    EventLoop m_ev;
    EventSubcriber m_sub;
};
