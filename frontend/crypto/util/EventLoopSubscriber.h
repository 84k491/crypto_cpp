#pragma once

#include "EventChannel.h"
#include "EventLoop.h"
#include "EventTimeseriesChannel.h"
#include "ISubsription.h"
#include "crossguid/guid.hpp"

#include <cstdlib>
#include <map>
#include <memory>

// class Subcriber
// {
// public:
//     Subcriber(const std::shared_ptr<EventLoop> & el)
//         : m_event_loop(el)
//     {
//         if (!m_event_loop) {
//             abort();
//         }
//     }
//
//     template <class EventChannelT, class UpdateCallbackT>
//     void subscribe(
//             EventChannelT & channel,
//             UpdateCallbackT && callback,
//             Priority priority = Priority::Normal)
//     {
//         const auto sub = channel.subscribe(m_event_loop, std::forward<UpdateCallbackT>(callback), priority);
//         m_subscriptions.push_back(sub);
//     }
//
// private:
//     std::shared_ptr<EventLoop> m_event_loop;
//
//     std::list<std::shared_ptr<ISubscription>> m_subscriptions; // those must be destroyed before EvLoop
// };

// Represents an event loop with all it's subscriptions
// Needed to guarantee that event loop will unsubscribe from all channels before destruction
class EventLoopSubscriber
{
public:
    EventLoopSubscriber()
        : m_event_loop(EventLoop::create())
    {
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

    template <class EventChannelT, class UpdateCallbackT>
    [[nodiscard]]
    std::shared_ptr<ISubscription> subscribe_for_sub(
            EventChannelT & channel,
            UpdateCallbackT && callback,
            Priority priority = Priority::Normal)
    {
        return channel.subscribe(m_event_loop, std::forward<UpdateCallbackT>(callback), priority);
    }

    template <class EventChannelT, class SnapshotCallbackT, class UpdateCallbackT>
    void subscribe(
            EventChannelT & channel,
            SnapshotCallbackT && snapshot_callback,
            UpdateCallbackT && update_callback,
            const std::string & bucket_name = "")
    {
        const auto sub = channel.subscribe(
                m_event_loop,
                std::forward<SnapshotCallbackT>(snapshot_callback),
                std::forward<UpdateCallbackT>(update_callback));
        m_subscriptions[bucket_name].push_back(sub);
    }

    void unsubscribe_all()
    {
        m_subscriptions.clear();
    }

private:
    std::shared_ptr<EventLoop> m_event_loop;
    std::map<std::string, std::list<std::shared_ptr<ISubscription>>> m_subscriptions; // those must be destroyed before EvLoop
};
