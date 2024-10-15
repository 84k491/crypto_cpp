#pragma once

#include "Events.h"
#include "ISubsription.h"
#include "Macros.h"
#include "Guarded.h"

#include <crossguid2/crossguid/guid.hpp>
#include <memory>
#include <vector>

template <typename ObjectT>
class EventPublisher;

template <typename EventT>
class EventSubscription final : public ISubsription
{
    friend class EventPublisher<EventT>;

public:
    EventSubscription(
            const std::shared_ptr<IEventConsumer<EventT>> & consumer,
            EventPublisher<EventT> & publisher,
            xg::Guid guid)
        : m_consumer(consumer)
        , m_publisher(&publisher)
        , m_guid(guid)
    {
    }

    ~EventSubscription() override
    {
        if (m_publisher) {
            m_publisher->unsubscribe(m_guid);
        }
    }

private:
    std::weak_ptr<IEventConsumer<EventT>> m_consumer;
    EventPublisher<EventT> * m_publisher;
    xg::Guid m_guid;
};

template <typename EventT>
class EventPublisher
{
public:
    EventPublisher() = default;
    ~EventPublisher();

    void push(const EventT & object);

    [[nodiscard]] std::shared_ptr<EventSubscription<EventT>>
    subscribe(const std::shared_ptr<IEventConsumer<EventT>> & consumer);
    void unsubscribe(xg::Guid guid);

private:
    Guarded<std::vector<std::tuple<
            xg::Guid,
            std::weak_ptr<EventSubscription<EventT>>>>>
            m_update_callbacks;
};

template <typename EventT>
void EventPublisher<EventT>::push(const EventT & object)
{
    // TODO erase if nullptr
    auto callbacks_lref = m_update_callbacks.lock();
    for (const auto & [uuid, wptr] : callbacks_lref.get()) {
        UNWRAP_CONTINUE(subscribtion, wptr.lock());
        UNWRAP_CONTINUE(consumer, subscribtion.m_consumer.lock());
        consumer.push(object);
    }
}

template <typename EventT>
std::shared_ptr<EventSubscription<EventT>>
EventPublisher<EventT>::subscribe(const std::shared_ptr<IEventConsumer<EventT>> & consumer)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventSubscription<EventT>>(consumer, *this, guid);

    auto callbacks_lref = m_update_callbacks.lock();
    callbacks_lref.get().push_back({guid, std::weak_ptr{sptr}});
    return sptr;
}

template <typename EventT>
void EventPublisher<EventT>::unsubscribe(xg::Guid guid)
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
EventPublisher<EventT>::~EventPublisher()
{
    auto callbacks_lref = m_update_callbacks.lock();
    for (const auto [guid, wptr] : callbacks_lref.get()) {
        if (auto sptr = wptr.lock()) {
            sptr->m_publisher = nullptr;
        }
    }
    callbacks_lref.get().clear();
}
