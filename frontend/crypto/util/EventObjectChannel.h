#pragma once

#include "EventLoop.h"
#include "Events.h"
#include "ISubsription.h"
#include "Macros.h"

#include <crossguid/guid.hpp>
#include <functional>
#include <memory>
#include <vector>

template <typename ObjectT>
class EventObjectChannel;

template <typename ObjectT>
class EventObjectSubscription final : public ISubscription
{
    friend class EventObjectChannel<ObjectT>;

public:
    EventObjectSubscription(
            const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
            EventObjectChannel<ObjectT> & channel,
            xg::Guid guid)
        : m_consumer(consumer)
        , m_channel(&channel)
        , m_guid(guid)
    {
    }

    ~EventObjectSubscription() override
    {
        if (m_channel) {
            m_channel->unsubscribe(m_guid);
        }
    }

private:
    std::weak_ptr<IEventConsumer<LambdaEvent>> m_consumer;
    EventObjectChannel<ObjectT> * m_channel; // TODO make it atomic
    xg::Guid m_guid;
};

template <typename ObjectT>
class EventObjectChannel
{
public:
    EventObjectChannel() = default;
    ~EventObjectChannel();

    void push(const ObjectT & object);
    void update(std::function<void(ObjectT &)> && update_callback);
    const ObjectT & get() const
    {
        // TODO make thread safe
        return m_data;
    }

    [[nodiscard]] std::shared_ptr<EventObjectSubscription<ObjectT>>
    subscribe(
            const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
            std::function<void(const ObjectT &)> && update_callback,
            Priority priority = Priority::Normal);
    void unsubscribe(xg::Guid guid);

private:
    ObjectT m_data;
    std::vector<std::tuple< // TODO make it thread-safe
            xg::Guid,
            std::function<void(const ObjectT &)>, // TODO move it to subscription
            std::weak_ptr<EventObjectSubscription<ObjectT>>>>
            m_update_callbacks;
};

template <typename ObjectT>
void EventObjectChannel<ObjectT>::push(const ObjectT & object)
{
    m_data = object;
    // TODO erase if nullptr
    for (const auto & [uuid, cb, wptr] : m_update_callbacks) {
        UNWRAP_CONTINUE(subscribtion, wptr.lock());
        UNWRAP_CONTINUE(consumer, subscribtion.m_consumer.lock());
        consumer.push(LambdaEvent{[cb, object] { cb(object); }, Priority::Normal});
    }
}

template <typename ObjectT>
void EventObjectChannel<ObjectT>::update(std::function<void(ObjectT &)> && update_callback)
{
    update_callback(m_data);
    // TODO erase if nullptr
    for (const auto & [uuid, cb, wptr] : m_update_callbacks) {
        UNWRAP_CONTINUE(subscribtion, wptr.lock());
        UNWRAP_CONTINUE(consumer, subscribtion.m_consumer.lock());
        consumer.push(LambdaEvent([cb, object = m_data] { cb(object); }, Priority::Normal));
    }
}

template <typename ObjectT>
std::shared_ptr<EventObjectSubscription<ObjectT>>
EventObjectChannel<ObjectT>::subscribe(
        const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
        std::function<void(const ObjectT &)> && update_callback,
        Priority) // TODO use priority?
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventObjectSubscription<ObjectT>>(consumer, *this, guid);

    m_update_callbacks.push_back({guid, std::move(update_callback), std::weak_ptr{sptr}});
    return sptr;
}

template <typename ObjectT>
void EventObjectChannel<ObjectT>::unsubscribe(xg::Guid guid)
{
    for (auto it = m_update_callbacks.begin(); it != m_update_callbacks.end(); ++it) {
        if (std::get<xg::Guid>(*it) == guid) {
            m_update_callbacks.erase(it);
            break;
        }
    }
}

template <typename ObjectT>
EventObjectChannel<ObjectT>::~EventObjectChannel()
{
    for (const auto [guid, cb, wptr] : m_update_callbacks) {
        if (auto sptr = wptr.lock()) {
            sptr->m_channel = nullptr;
        }
    }
    m_update_callbacks.clear();
}
