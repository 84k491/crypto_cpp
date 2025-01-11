#pragma once

#include "EventLoop.h"
#include "Events.h"
#include "Guarded.h"
#include "ISubsription.h"
#include "Macros.h"

#include <chrono>
#include <crossguid2/crossguid/guid.hpp>
#include <functional>
#include <list>
#include <memory>
#include <vector>

template <typename ObjectT>
class EventTimeseriesChannel;

template <typename ObjectT>
class EventTimeseriesSubsription final : public ISubsription
{
    friend class EventTimeseriesChannel<ObjectT>;

public:
    EventTimeseriesSubsription(const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
                               EventTimeseriesChannel<ObjectT> & channel,
                               xg::Guid guid)
        : m_consumer(consumer)
        , m_channel(&channel)
        , m_guid(guid)
    {
    }

    ~EventTimeseriesSubsription() override
    {
        if (m_channel) {
            m_channel->unsubscribe(m_guid);
        }
    }

private:
    std::weak_ptr<IEventConsumer<LambdaEvent>> m_consumer;
    EventTimeseriesChannel<ObjectT> * m_channel;
    xg::Guid m_guid;
};

template <typename ObjectT>
class EventTimeseriesChannel
{
public:
    using TimeT = std::chrono::milliseconds;

    EventTimeseriesChannel() = default;
    ~EventTimeseriesChannel();

    void push(TimeT timestamp, const ObjectT & object);
    [[nodiscard]] std::shared_ptr<EventTimeseriesSubsription<ObjectT>> subscribe(
            const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
            std::function<void(const std::list<std::pair<TimeT, ObjectT>> &)> && snapshot_callback,
            std::function<void(TimeT, const ObjectT &)> && increment_callback);

    // thread unsafe
    void set_capacity(std::optional<std::chrono::milliseconds> capacity) { m_capacity = capacity; }

    void unsubscribe(xg::Guid guid);

private:
    struct SubscriberData
    {
        xg::Guid guid;
        std::function<void(TimeT, const ObjectT &)> callback;
        std::weak_ptr<EventTimeseriesSubsription<ObjectT>> wptr;
    };

private:
    Guarded<std::list<std::pair<TimeT, ObjectT>>> m_data;
    Guarded<std::list<SubscriberData>> m_increment_callbacks;
    std::optional<std::chrono::milliseconds> m_capacity;
};

template <typename ObjectT>
void EventTimeseriesChannel<ObjectT>::push(EventTimeseriesChannel::TimeT timestamp, const ObjectT & object)
{
    {
        auto data_lref = m_data.lock();
        auto & data = data_lref.get();
        while (m_capacity.has_value() && !data.empty() && data.front().first < timestamp - *m_capacity) {
            data.pop_front();
        }
        data.emplace_back(timestamp, object);
    }

    auto callbacks_lref = m_increment_callbacks.lock();
    for (const auto & el : callbacks_lref.get()) {
        UNWRAP_CONTINUE(subscribtion, el.wptr.lock());
        UNWRAP_CONTINUE(consumer, subscribtion.m_consumer.lock());
        consumer.push(LambdaEvent(
                [el,
                 timestamp,
                 object] { el.callback(timestamp, object); }));
    }
}

template <typename ObjectT>
std::shared_ptr<EventTimeseriesSubsription<ObjectT>> EventTimeseriesChannel<ObjectT>::subscribe(
        const std::shared_ptr<IEventConsumer<LambdaEvent>> & consumer,
        std::function<void(const std::list<std::pair<TimeT, ObjectT>> &)> && snapshot_callback,
        std::function<void(TimeT, const ObjectT &)> && increment_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventTimeseriesSubsription<ObjectT>>(consumer, *this, guid);

    m_increment_callbacks.lock().get().emplace_back(guid, std::move(increment_callback), std::weak_ptr{sptr});
    auto data_lref = m_data.lock();
    consumer->push(LambdaEvent(
            [cb = std::move(snapshot_callback),
             d = data_lref.get()] { cb(d); }));

    return sptr;
}

template <typename ObjectT>
void EventTimeseriesChannel<ObjectT>::unsubscribe(xg::Guid guid)
{
    auto consumers_lref = m_increment_callbacks.lock();
    auto & consumers = consumers_lref.get();
    for (auto it = consumers.begin(); it != consumers.end(); ++it) {
        if (it->guid == guid) {
            consumers.erase(it);
            break;
        }
    }
}

template <typename ObjectT>
EventTimeseriesChannel<ObjectT>::~EventTimeseriesChannel()
{
    auto consumers_lref = m_increment_callbacks.lock();
    for (auto & el : consumers_lref.get()) {
        UNWRAP_CONTINUE(subscribtion, el.wptr.lock());
        subscribtion.m_channel = nullptr;
    }
    consumers_lref.get().clear();
}
