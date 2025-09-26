#pragma once

#include "EventChannel.h"
#include "EventLoop.h"
#include "EventTimeseriesChannel.h"
#include "ISubsription.h"

#include <cstdlib>
#include <memory>

class EventSubcriber
{
public:
    EventSubcriber(const std::shared_ptr<EventLoop> & el)
        : m_event_loop(el)
    {
        if (!m_event_loop) {
            abort();
        }
    }

    template <class EventChannelT, class UpdateCallbackT>
    void subscribe(
            EventChannelT & channel,
            UpdateCallbackT && callback,
            Priority priority = Priority::Normal)
    {
        const auto sub = channel.subscribe(m_event_loop, std::forward<UpdateCallbackT>(callback), priority);
        m_subscriptions.push_back(sub);
    }

    template <class EventTimeseriesChannelT, class SnapshotCallbackT, class UpdateCallbackT>
    void subscribe(
            EventTimeseriesChannelT & channel,
            SnapshotCallbackT && snapshot_callback,
            UpdateCallbackT && update_callback)
    {
        const auto sub = channel.subscribe(
                m_event_loop,
                std::forward<SnapshotCallbackT>(snapshot_callback),
                std::forward<UpdateCallbackT>(update_callback));
        m_subscriptions.push_back(sub);
    }

private:
    std::shared_ptr<EventLoop> m_event_loop;

    std::list<std::shared_ptr<ISubscription>> m_subscriptions; // those must be destroyed before EvLoop
};
