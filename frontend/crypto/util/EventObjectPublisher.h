#pragma once

#include "Events.h"
#include "ISubsription.h"
#include "Macros.h"

#include <crossguid2/crossguid/guid.hpp>
#include <functional>
#include <memory>
#include <vector>

template <typename ObjectT>
class EventObjectPublisher;

template <typename ObjectT>
class EventObjectSubscription final : public ISubsription
{
    friend class EventObjectPublisher<ObjectT>;

public:
    EventObjectSubscription(
            const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
            EventObjectPublisher<ObjectT> & publisher,
            xg::Guid guid)
        : m_consumer(consumer)
        , m_publisher(&publisher)
        , m_guid(guid)
    {
    }

    ~EventObjectSubscription() override
    {
        if (m_publisher) {
            m_publisher->unsubscribe(m_guid);
        }
    }

private:
    std::weak_ptr<IEventConsumer<LambdaEvent>> m_consumer;
    EventObjectPublisher<ObjectT> * m_publisher; // TODO make it atomic
    xg::Guid m_guid;
};

template <typename ObjectT>
class EventObjectPublisher // TODO rename it to object pub
{
public:
    EventObjectPublisher() = default;
    ~EventObjectPublisher();

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
            std::function<void(const ObjectT &)> && update_callback);
    void unsubscribe(xg::Guid guid);

private:
    ObjectT m_data;
    std::vector<std::tuple< // TODO make it thread-safe
            xg::Guid,
            std::function<void(const ObjectT &)>,
            std::weak_ptr<EventObjectSubscription<ObjectT>>>>
            m_update_callbacks;
};

template <typename ObjectT>
void EventObjectPublisher<ObjectT>::push(const ObjectT & object)
{
    m_data = object;
    // TODO erase if nullptr
    for (const auto & [uuid, cb, wptr] : m_update_callbacks) {
        UNWRAP_CONTINUE(subscribtion, wptr.lock());
        UNWRAP_CONTINUE(consumer, subscribtion.m_consumer.lock());
        consumer.push(LambdaEvent([cb, object] { cb(object); }));
    }
}

template <typename ObjectT>
void EventObjectPublisher<ObjectT>::update(std::function<void(ObjectT &)> && update_callback)
{
    update_callback(m_data);
    // TODO erase if nullptr
    for (const auto & [uuid, cb, wptr] : m_update_callbacks) {
        UNWRAP_CONTINUE(subscribtion, wptr.lock());
        UNWRAP_CONTINUE(consumer, subscribtion.m_consumer.lock());
        consumer.push(LambdaEvent([cb, object = m_data] { cb(object); }));
    }
}

template <typename ObjectT>
std::shared_ptr<EventObjectSubscription<ObjectT>>
EventObjectPublisher<ObjectT>::subscribe(
        const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
        std::function<void(const ObjectT &)> && update_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventObjectSubscription<ObjectT>>(consumer, *this, guid);

    m_update_callbacks.push_back({guid, std::move(update_callback), std::weak_ptr{sptr}});
    return sptr;
}

template <typename ObjectT>
void EventObjectPublisher<ObjectT>::unsubscribe(xg::Guid guid)
{
    for (auto it = m_update_callbacks.begin(); it != m_update_callbacks.end(); ++it) {
        if (std::get<xg::Guid>(*it) == guid) {
            m_update_callbacks.erase(it);
            break;
        }
    }
}

template <typename ObjectT>
EventObjectPublisher<ObjectT>::~EventObjectPublisher()
{
    for (const auto [guid, cb, wptr] : m_update_callbacks) {
        if (auto sptr = wptr.lock()) {
            sptr->m_publisher = nullptr;
        }
    }
    m_update_callbacks.clear();
}
