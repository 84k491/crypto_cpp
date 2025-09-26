#pragma once

#include "EventLoop.h"
#include "EventLoopSubscriber.h"
#include "Scheduler.h"

#include <utility>

class EventLoopWithDelays : public ILambdaAcceptor
{
public:
    EventLoopWithDelays()
        : m_sub(m_ev) // TODO inherit from EL, pass it here by ref
    {
        auto & ch = Scheduler::i().delayed_channel(m_guid);
        m_sub.subscribe(
                ch,
                [this](LambdaEvent ev) {
                    m_ev->push_to_queue(std::move(ev));
                });
    }

    bool push_to_queue(LambdaEvent event) override
    {
        return m_ev->push_to_queue(std::move(event));
    }

    bool push_to_queue_delayed(std::chrono::milliseconds delay, LambdaEvent value) override
    {
        Scheduler::i().delay_event(m_guid, delay, value);
        return true;
    }

private:
    xg::Guid m_guid;

    std::shared_ptr<EventLoop> m_ev;
    EventSubcriber m_sub;
};
