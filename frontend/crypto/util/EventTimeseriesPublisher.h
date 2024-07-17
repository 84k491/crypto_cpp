#pragma once

#include "Events.h"
#include "ISubsription.h"

#include <chrono>
#include <crossguid2/crossguid/guid.hpp>
#include <functional>
#include <list>
#include <memory>
#include <vector>

template <typename ObjectT>
class EventTimeseriesPublisher;

template <typename ObjectT>
class EventTimeseriesSubsription final : public ISubsription
{
    friend class EventTimeseriesPublisher<ObjectT>;

public:
    EventTimeseriesSubsription(IEventConsumer<LambdaEvent> & consumer,
                               EventTimeseriesPublisher<ObjectT> & publisher,
                               xg::Guid guid)
        : m_consumer(consumer)
        , m_publisher(&publisher)
        , m_guid(guid)
    {
    }

    ~EventTimeseriesSubsription() override
    {
        if (m_publisher) {
            m_publisher->unsubscribe(m_guid);
        }
    }

private:
    IEventConsumer<LambdaEvent> & m_consumer; // TODO referenced object can be destroyed before the subscription
    EventTimeseriesPublisher<ObjectT> * m_publisher;
    xg::Guid m_guid;
};

template <typename ObjectT>
class EventTimeseriesPublisher
{
public:
    using TimeT = std::chrono::milliseconds;

    EventTimeseriesPublisher() = default;
    ~EventTimeseriesPublisher();

    void push(TimeT timestamp, const ObjectT & object);
    [[nodiscard]] std::shared_ptr<EventTimeseriesSubsription<ObjectT>> subscribe(
            IEventConsumer<LambdaEvent> & consumer,
            std::function<void(const std::vector<std::pair<TimeT, ObjectT>> &)> && snapshot_callback,
            std::function<void(TimeT, const ObjectT &)> && increment_callback);

    void unsubscribe(xg::Guid guid);

private:
    std::vector<std::pair<TimeT, ObjectT>> m_data;
    std::list<std::tuple<
            xg::Guid,
            std::function<void(TimeT, const ObjectT &)>,
            std::weak_ptr<EventTimeseriesSubsription<ObjectT>>>>
            m_increment_callbacks;
};

template <typename ObjectT>
void EventTimeseriesPublisher<ObjectT>::push(EventTimeseriesPublisher::TimeT timestamp, const ObjectT & object)
{
    m_data.emplace_back(timestamp, object);
    for (const auto & [uuid, cb, wptr] : m_increment_callbacks) {
        const std::shared_ptr<EventTimeseriesSubsription<ObjectT>> sptr = wptr.lock();
        sptr->m_consumer.push(LambdaEvent(
                [cb,
                 timestamp,
                 object] { cb(timestamp, object); }));
    }
}

template <typename ObjectT>
std::shared_ptr<EventTimeseriesSubsription<ObjectT>> EventTimeseriesPublisher<ObjectT>::subscribe(
        IEventConsumer<LambdaEvent> & consumer,
        std::function<void(const std::vector<std::pair<TimeT, ObjectT>> &)> && snapshot_callback,
        std::function<void(TimeT, const ObjectT &)> && increment_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventTimeseriesSubsription<ObjectT>>(consumer, *this, guid);

    m_increment_callbacks.emplace_back(guid, std::move(increment_callback), std::weak_ptr{sptr});
    consumer.push(LambdaEvent(
            [cb = std::move(snapshot_callback),
             d = m_data] { cb(d); }));

    return sptr;
}

template <typename ObjectT>
void EventTimeseriesPublisher<ObjectT>::unsubscribe(xg::Guid guid)
{
    for (auto it = m_increment_callbacks.begin(); it != m_increment_callbacks.end(); ++it) {
        if (std::get<xg::Guid>(*it) == guid) {
            m_increment_callbacks.erase(it);
            break;
        }
    }
}

template <typename ObjectT>
EventTimeseriesPublisher<ObjectT>::~EventTimeseriesPublisher()
{
    for (auto & [uuid, _, wptr] : m_increment_callbacks) {
        if (auto sptr = wptr.lock()) {
            sptr->m_publisher = nullptr;
        }
    }
    m_increment_callbacks.clear();
}
