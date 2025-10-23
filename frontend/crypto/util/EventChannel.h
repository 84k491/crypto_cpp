#pragma once

#include "ILambdaAcceptor.h"
#include "Events.h"
#include "Guarded.h"
#include "ISubsription.h"
#include "Macros.h"

#include <crossguid/guid.hpp>
#include <list>
#include <memory>

template <typename ObjectT>
class EventChannel;

template <typename EventT>
class EventSubscription final : public ISubscription
{
    friend class EventChannel<EventT>;

public:
    EventSubscription(
            ILambdaAcceptor & consumer,
            std::function<void(const EventT &)> callback,
            Priority priority,
            EventChannel<EventT> & channel,
            xg::Guid guid)
        : m_consumer(consumer)
        , m_callback(callback)
        , m_priority(priority)
        , m_channel(&channel)
        , m_guid(guid)
    {
    }

    auto guid() const { return m_guid; }

    ~EventSubscription() override
    {
        if (m_channel) {
            m_channel->unsubscribe(m_guid);
        }
    }

private:
    ILambdaAcceptor & m_consumer;
    std::function<void(const EventT &)> m_callback;
    Priority m_priority;

    // ptr because channel can be destroyed in another thread before sub
    EventChannel<EventT> * m_channel;
    xg::Guid m_guid;
};

//! A channel for sequential events. No state saved
template <typename EventT>
class EventChannel
{
public:
    EventChannel() = default;
    ~EventChannel();

    void push(const EventT & object);
    void push_delayed(const EventT & object, std::chrono::milliseconds delay);

    [[nodiscard]] std::shared_ptr<EventSubscription<EventT>>
    subscribe(
            ILambdaAcceptor & consumer,
            std::function<void(const EventT &)> && update_callback,
            Priority priority = Priority::Normal);
    void unsubscribe(xg::Guid guid);

private:
    using SubWPtr = std::weak_ptr<EventSubscription<EventT>>;
    // Guid is here because wptr will be null on unsub on dtor
    Guarded<std::list<std::pair<xg::Guid, SubWPtr>>> m_subscriptions;
};

template <typename EventT>
void EventChannel<EventT>::push(const EventT & object)
{
    auto subs_lref = m_subscriptions.lock();

    for (const auto & [_, wptr] : subs_lref.get()) {

        // TODO nodiscard on sub
        // it can't be nullptr, unsub always happens before
        UNWRAP_CONTINUE(subsription, wptr.lock());

        subsription.m_consumer.push(
                LambdaEvent{
                        [cb = subsription.m_callback, object] { cb(object); },
                        subsription.m_priority});
    }
}

template <typename EventT>
void EventChannel<EventT>::push_delayed(const EventT & object, std::chrono::milliseconds delay)
{
    auto subs_lref = m_subscriptions.lock();

    for (const auto & [_, wptr] : subs_lref.get()) {

        // it can't be nullptr, unsub always happens before
        UNWRAP_CONTINUE(subsription, wptr.lock());

        subsription.m_consumer.push_delayed(
                delay,
                LambdaEvent{
                        [cb = subsription.m_callback, object] { cb(object); },
                        subsription.m_priority});
    }
}

// TODO callback's argument type can be different from channel's EventT. Make a static assert to weld them
template <typename EventT>
std::shared_ptr<EventSubscription<EventT>>
EventChannel<EventT>::subscribe(
        ILambdaAcceptor & consumer,
        std::function<void(const EventT &)> && update_callback,
        Priority priority)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventSubscription<EventT>>(consumer, update_callback, priority, *this, guid);

    auto subs_lref = m_subscriptions.lock();
    subs_lref.get().push_back(std::make_pair(guid, std::weak_ptr{sptr}));

    return sptr;
}

template <typename EventT>
void EventChannel<EventT>::unsubscribe(xg::Guid guid)
{
    auto subs_lref = m_subscriptions.lock();
    auto & callbacks = subs_lref.get();

    for (auto it = callbacks.begin(); it != callbacks.end(); ++it) {
        auto & [sub_guid, wptr] = *it;

        if (sub_guid == guid) {
            callbacks.erase(it);
            break;
        }
    }
}

template <typename EventT>
EventChannel<EventT>::~EventChannel()
{
    auto subs_lref = m_subscriptions.lock();

    for (auto & [_, wptr] : subs_lref.get()) {
        if (auto sptr = wptr.lock()) {
            sptr->m_channel = nullptr;
        }
    }
    subs_lref.get().clear();
}
