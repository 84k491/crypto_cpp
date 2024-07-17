#pragma once

#include "Events.h"
#include "ISubsription.h"

#include <crossguid2/crossguid/guid.hpp>
#include <functional>
#include <memory>
#include <vector>

template <typename ObjectT>
class EventObjectPublisher;

template <typename ObjectT>
class EventObjectSubscribtion final : public ISubsription // TODO rename to SubsriPtion
{
    friend class EventObjectPublisher<ObjectT>;

public:
    EventObjectSubscribtion(
            IEventConsumer<LambdaEvent> & consumer,
            EventObjectPublisher<ObjectT> & publisher,
            xg::Guid guid)
        : m_consumer(consumer)
        , m_publisher(&publisher)
        , m_guid(guid)
    {
    }

    ~EventObjectSubscribtion() override
    {
        if (m_publisher) {
            m_publisher->unsubscribe(m_guid);
        }
    }

private:
    IEventConsumer<LambdaEvent> & m_consumer; // TODO referenced object can be destroyed before the subscription
    EventObjectPublisher<ObjectT> * m_publisher;
    xg::Guid m_guid;
};

template <typename ObjectT>
class EventObjectPublisher
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

    [[nodiscard]] std::shared_ptr<EventObjectSubscribtion<ObjectT>>
    subscribe(
            IEventConsumer<LambdaEvent> & consumer,
            std::function<void(const ObjectT &)> && update_callback);
    void unsubscribe(xg::Guid guid);

private:
    ObjectT m_data;
    std::vector<std::tuple<
            xg::Guid,
            std::function<void(const ObjectT &)>,
            std::weak_ptr<EventObjectSubscribtion<ObjectT>>>>
            m_update_callbacks;
};

template <typename ObjectT>
void EventObjectPublisher<ObjectT>::push(const ObjectT & object)
{
    m_data = object;
    for (const auto & [uuid, cb, wptr] : m_update_callbacks) {
        const std::shared_ptr<EventObjectSubscribtion<ObjectT>> sptr = wptr.lock();
        sptr->m_consumer.push(LambdaEvent([cb, object] { cb(object); }));
    }
}

template <typename ObjectT>
void EventObjectPublisher<ObjectT>::update(std::function<void(ObjectT &)> && update_callback)
{
    update_callback(m_data);
    for (const auto & [uuid, cb, wptr] : m_update_callbacks) {
        const std::shared_ptr<EventObjectSubscribtion<ObjectT>> sptr = wptr.lock();
        sptr->m_consumer.push(LambdaEvent([cb, object = m_data] { cb(object); }));
    }
}

template <typename ObjectT>
std::shared_ptr<EventObjectSubscribtion<ObjectT>>
EventObjectPublisher<ObjectT>::subscribe(
        IEventConsumer<LambdaEvent> & consumer,
        std::function<void(const ObjectT &)> && update_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventObjectSubscribtion<ObjectT>>(consumer, *this, guid);

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
