#pragma once

#include "ILambdaAcceptor.h"
#include "ISubsription.h"

#include <list>
#include <memory>

class EventSubcriber
{
public:
    EventSubcriber(ILambdaAcceptor & el)
        : m_event_loop(el)
    {
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
    ILambdaAcceptor & m_event_loop;

    std::list<std::shared_ptr<ISubscription>> m_subscriptions; // those must be destroyed before EvLoop
};
