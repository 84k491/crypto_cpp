#pragma once

#include "EventLoop.h"
#include "EventPublisher.h"
#include "EventTimeseriesPublisher.h"
#include "ISubsription.h"

#include <map>
#include <memory>

// Represents an event loop with all it's subscriptions
// Needed to guarantee that event loop will unsubscribe from all channels before destruction
template <class... Args>
class EventLoopSubscriber
{
public:
    EventLoopSubscriber(IEventInvoker<Args...> & invoker)
        : m_event_loop(EventLoop<Args...>::create(&invoker))
    {
    }

    template <class EventT>
    void push_event(EventT ev)
    {
        static_cast<IEventConsumer<EventT> &>(*m_event_loop).push(ev);
    }

    template <class EventT>
    void push_delayed(std::chrono::milliseconds t, EventT ev)
    {
        static_cast<IEventConsumer<EventT> &>(*m_event_loop).push_delayed(t, ev);
    }

    template <class EventChannelT>
    void subscribe(EventChannelT & channel, const std::string & bucket_name = "")
    {
        const auto sub = channel.subscribe(m_event_loop);
        m_subscriptions[bucket_name].push_back(sub);
    }

    template <class EventChannelT, class UpdateCallbackT>
    void subscribe(EventChannelT & channel, UpdateCallbackT && callback, const std::string & bucket_name = "")
    {
        const auto sub = channel.subscribe(m_event_loop, std::forward<UpdateCallbackT>(callback));
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

private:
    std::shared_ptr<EventLoop<Args...>> m_event_loop;
    std::map<std::string, std::list<std::shared_ptr<ISubsription>>> m_subscriptions; // those must be destroyed before EvLoop
};
