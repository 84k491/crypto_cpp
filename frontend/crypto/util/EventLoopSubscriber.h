#pragma once

#include "EventChannel.h"
#include "EventLoop.h"
#include "EventTimeseriesChannel.h"
#include "ISubsription.h"
#include "crossguid/guid.hpp"

#include <map>
#include <memory>

// Represents an event loop with all it's subscriptions
// Needed to guarantee that event loop will unsubscribe from all channels before destruction
class EventLoopSubscriber
{
public:
    EventLoopSubscriber()
        : m_event_loop(EventLoop::create())
    {
    }

    template <class EventT>
    void push_event(EventT ev)
    {
        static_cast<ILambdaAcceptor &>(*m_event_loop).push(ev);
    }

    template <class EventT>
    void push_delayed(std::chrono::milliseconds t, EventT ev)
    {
        static_cast<ILambdaAcceptor &>(*m_event_loop).push_delayed(t, ev);
    }

    template <class EventChannelT>
    void subscribe(EventChannelT & channel, const std::string & bucket_name = "")
    {
        const auto sub = channel.subscribe(m_event_loop);
        m_subscriptions[bucket_name].push_back(sub);
    }

    template <class EventChannelT, class UpdateCallbackT>
    void subscribe(
            EventChannelT & channel,
            UpdateCallbackT && callback,
            Priority priority = Priority::Normal,
            const std::string & bucket_name = "")
    {
        const auto sub = channel.subscribe(m_event_loop, std::forward<UpdateCallbackT>(callback), priority);
        m_subscriptions[bucket_name].push_back(sub);
    }

    void unsubscribe(const std::string & bucket_name)
    {
        m_subscriptions.erase(bucket_name);
    }

    void unsubscribe_all()
    {
        m_subscriptions.clear();
    }

public:
    std::shared_ptr<EventLoop> m_event_loop;

private:
    std::map<std::string, std::list<std::shared_ptr<ISubscription>>> m_subscriptions; // those must be destroyed before EvLoop
};
