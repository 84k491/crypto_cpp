#pragma once

#include "ILambdaAcceptor.h"
#include "ISubsription.h"
#include "crossguid/guid.hpp"

#include <list>
#include <memory>

/*
    This class is used to hold user's subscriptions for channels.
    Events are pushed to a specified EventLoop.
    Automatically unsubscribes from all subscriptions on destruction
    Can be destructed in an EventLoop's callback
    Dangled events are discarded after destruction

    Uses a guarantee: EventSubcriber will be destructed before any object that used in its callbacks
    Uses a guarantee: EventLoop's lifetime is bigger than EventSubcriber's

    Provides a guarantee: No Event will be executed after EventSubcriber's destructor

    Known race: UB if an Event is being called in one thread and EventSubcriber is being destructed in another
*/
class EventSubcriber
{
public:
    EventSubcriber(ILambdaAcceptor & el)
        : m_guid{xg::newGuid()}
        , m_event_loop{el}
    {
    }

    ~EventSubcriber()
    {
        m_subscriptions.clear();
        m_event_loop.discard_subscriber_events(m_guid);
    }

    template <class EventChannelT, class UpdateCallbackT>
    void subscribe(
            EventChannelT & channel,
            UpdateCallbackT && callback,
            Priority priority = Priority::Normal)
    {
        const auto sub = channel.subscribe(
                m_event_loop,
                m_guid,
                std::forward<UpdateCallbackT>(callback),
                priority);
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
                m_guid,
                std::forward<SnapshotCallbackT>(snapshot_callback),
                std::forward<UpdateCallbackT>(update_callback));
        m_subscriptions.push_back(sub);
    }

private:
    xg::Guid m_guid;

    ILambdaAcceptor & m_event_loop;

    std::list<std::shared_ptr<ISubscription>> m_subscriptions;
};
