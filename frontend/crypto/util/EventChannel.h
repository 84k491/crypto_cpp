#pragma once

#include "EventLoop.h"
#include "Guarded.h"
#include "ISubsription.h"
#include "Macros.h"
#include "Events.h"

#include <crossguid/guid.hpp>
#include <memory>
#include <vector>

template <typename ObjectT>
class EventChannel;

template <typename EventT>
class EventSubscription final : public ISubscription
{
    friend class EventChannel<EventT>;

public:
    EventSubscription(
            const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
            std::function<void(const EventT &)> callback,
            EventChannel<EventT> & channel,
            xg::Guid guid)
        : m_consumer(consumer)
        , m_callback(callback)
        , m_channel(&channel)
        , m_guid(guid)
    {
    }

    ~EventSubscription() override
    {
        if (m_channel) {
            m_channel->unsubscribe(m_guid);
        }
    }

private:
    std::weak_ptr<IEventConsumer<LambdaEvent>> m_consumer;
    std::function<void(const EventT &)> m_callback;
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

    [[nodiscard]] std::shared_ptr<EventSubscription<EventT>>
    subscribe(
            const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
            std::function<void(const EventT &)> && update_callback);
    void unsubscribe(xg::Guid guid);

private:
    Guarded<std::vector<std::tuple<
            xg::Guid,
            std::weak_ptr<EventSubscription<EventT>>>>>
            m_update_callbacks;
};

template <typename EventT>
void EventChannel<EventT>::push(const EventT & object)
{
    // TODO erase if nullptr
    auto callbacks_lref = m_update_callbacks.lock();
    for (const auto & [uuid, wptr] : callbacks_lref.get()) {
        UNWRAP_CONTINUE(subscribtion, wptr.lock());
        UNWRAP_CONTINUE(consumer, subscribtion.m_consumer.lock());
        consumer.push(LambdaEvent([cb = subscribtion.m_callback, object] { cb(object); }));
    }
}

template <typename EventT>
std::shared_ptr<EventSubscription<EventT>>
EventChannel<EventT>::subscribe(
        const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
        std::function<void(const EventT &)> && update_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventSubscription<EventT>>(consumer, update_callback, *this, guid);

    auto callbacks_lref = m_update_callbacks.lock();
    callbacks_lref.get().push_back({guid, std::weak_ptr{sptr}});
    return sptr;
}

template <typename EventT>
void EventChannel<EventT>::unsubscribe(xg::Guid guid)
{
    auto callbacks_lref = m_update_callbacks.lock();
    for (auto it = callbacks_lref.get().begin(); it != callbacks_lref.get().end(); ++it) {
        if (std::get<xg::Guid>(*it) == guid) {
            callbacks_lref.get().erase(it);
            break;
        }
    }
}

template <typename EventT>
EventChannel<EventT>::~EventChannel()
{
    auto callbacks_lref = m_update_callbacks.lock();
    for (const auto [guid, wptr] : callbacks_lref.get()) {
        if (auto sptr = wptr.lock()) {
            sptr->m_channel = nullptr;
        }
    }
    callbacks_lref.get().clear();
}
