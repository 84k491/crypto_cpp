#pragma once

#include "EventLoop.h"
#include "Events.h"
#include "Guarded.h"
#include "ISubsription.h"
#include "Macros.h"

#include <algorithm>
#include <chrono>
#include <crossguid/guid.hpp>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <optional>

template <typename ObjectT>
class EventTimeseriesChannel;

template <typename ObjectT>
class EventTimeseriesSubsription final : public ISubscription
{
    using TimeT = std::chrono::milliseconds;
    friend class EventTimeseriesChannel<ObjectT>;

public:
    EventTimeseriesSubsription(
            ILambdaAcceptor & consumer,
            std::function<void(TimeT, const ObjectT &)> callback,
            EventTimeseriesChannel<ObjectT> & channel,
            xg::Guid subscriber_guid,
            xg::Guid guid)
        : m_consumer(consumer)
        , m_callback(callback)
        , m_channel(&channel)
        , m_subscriber_guid(subscriber_guid)
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
    ILambdaAcceptor & m_consumer;
    std::function<void(TimeT, const ObjectT &)> m_callback;
    EventTimeseriesChannel<ObjectT> * m_channel;
    xg::Guid m_subscriber_guid;
    xg::Guid m_guid;
};

template <typename ObjectT>
class EventTimeseriesChannel
{
public:
    using TimeT = std::chrono::milliseconds;
    using SnapshotCallbackT = std::function<void(const std::deque<std::pair<TimeT, ObjectT>> &)>;

    EventTimeseriesChannel() = default;
    EventTimeseriesChannel(EventTimeseriesChannel &) = delete;
    ~EventTimeseriesChannel();

    void push(TimeT timestamp, const ObjectT & object);

    [[nodiscard]] std::shared_ptr<EventTimeseriesSubsription<ObjectT>> subscribe(
            ILambdaAcceptor & consumer,
            xg::Guid subcriber_guid,
            SnapshotCallbackT && snapshot_callback,
            std::function<void(TimeT, const ObjectT &)> && increment_callback);

    // thread unsafe
    void set_capacity(std::optional<std::chrono::milliseconds> capacity) { m_capacity = capacity; }

    std::deque<std::pair<TimeT, ObjectT>> data_copy() { return m_data.lock().get(); }
    std::deque<std::pair<TimeT, ObjectT>> data_copy_in_range(TimeT start, TimeT end, bool with_last_point_before_start = false);
    std::optional<std::pair<TimeT, TimeT>> first_and_last_timestamps();

    void unsubscribe(xg::Guid guid);

private:
    Guarded<std::deque<std::pair<TimeT, ObjectT>>> m_data;

    using SubWPtr = std::weak_ptr<EventTimeseriesSubsription<ObjectT>>;
    // Guid is here because wptr will be null on unsub on dtor
    Guarded<std::list<std::pair<xg::Guid, SubWPtr>>> m_subscriptions;

    std::optional<std::chrono::milliseconds> m_capacity;
};

template <typename ObjectT>
void EventTimeseriesChannel<ObjectT>::push(EventTimeseriesChannel::TimeT timestamp, const ObjectT & object)
{
    // same lock order as in subscribe, so a push can't interleave with a snapshot
    auto subs_lref = m_subscriptions.lock();
    {
        auto data_lref = m_data.lock();
        auto & data = data_lref.get();
        while (m_capacity.has_value() && !data.empty() && data.front().first < timestamp - *m_capacity) {
            data.pop_front();
        }
        data.emplace_back(timestamp, object);
    }

    for (const auto & [_, sub_wptr] : subs_lref.get()) {

        // TODO nodiscard on sub
        // it can't be nullptr, unsub always happens before
        UNWRAP_CONTINUE(subscribtion, sub_wptr.lock());

        subscribtion.m_consumer.push(LambdaEvent{
                subscribtion.m_subscriber_guid,
                [cb = subscribtion.m_callback,
                 timestamp,
                 object] { cb(timestamp, object); },
                Priority::Normal});
    }
}

template <typename ObjectT>
std::shared_ptr<EventTimeseriesSubsription<ObjectT>> EventTimeseriesChannel<ObjectT>::subscribe(
        ILambdaAcceptor & consumer,
        xg::Guid subcriber_guid,
        SnapshotCallbackT && snapshot_callback,
        std::function<void(TimeT, const ObjectT &)> && increment_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventTimeseriesSubsription<ObjectT>>(
            consumer,
            increment_callback,
            *this,
            subcriber_guid,
            guid);

    auto subs_lref = m_subscriptions.lock();
    subs_lref.get().emplace_back(std::make_pair(guid, std::weak_ptr{sptr}));

    // don't copy snapshot data if there is no snapshot callback
    if (!snapshot_callback) {
        return sptr;
    }

    auto data_lref = m_data.lock();

    consumer.push(LambdaEvent{
            subcriber_guid,
            [cb = std::move(snapshot_callback),
             d = data_lref.get()] { cb(d); },
            Priority::Normal});

    return sptr;
}

template <typename ObjectT>
std::deque<std::pair<std::chrono::milliseconds, ObjectT>> EventTimeseriesChannel<ObjectT>::data_copy_in_range(TimeT start, TimeT end, bool with_last_point_before_start)
{
    auto data_lref = m_data.lock();
    const auto & data = data_lref.get();

    const auto ts_less = [](const std::pair<TimeT, ObjectT> & point, TimeT ts) { return point.first < ts; };
    const auto less_ts = [](TimeT ts, const std::pair<TimeT, ObjectT> & point) { return ts < point.first; };

    auto begin = std::lower_bound(data.begin(), data.end(), start, ts_less);
    if (with_last_point_before_start && begin != data.begin()) {
        --begin;
    }
    const auto finish = std::upper_bound(begin, data.end(), end, less_ts);

    return std::deque<std::pair<TimeT, ObjectT>>(begin, finish);
}

template <typename ObjectT>
std::optional<std::pair<std::chrono::milliseconds, std::chrono::milliseconds>> EventTimeseriesChannel<ObjectT>::first_and_last_timestamps()
{
    auto data_lref = m_data.lock();
    const auto & data = data_lref.get();
    if (data.empty()) {
        return std::nullopt;
    }
    return std::make_pair(data.front().first, data.back().first);
}

template <typename ObjectT>
void EventTimeseriesChannel<ObjectT>::unsubscribe(xg::Guid guid)
{
    auto subs_lref = m_subscriptions.lock();
    auto & consumers = subs_lref.get();

    for (auto it = consumers.begin(); it != consumers.end(); ++it) {
        auto & [sub_guid, _] = *it;

        if (sub_guid == guid) {
            consumers.erase(it);
            break;
        }
    }
}

template <typename ObjectT>
EventTimeseriesChannel<ObjectT>::~EventTimeseriesChannel()
{
    auto subs_lref = m_subscriptions.lock();

    for (auto & [_, sub_wptr] : subs_lref.get()) {
        UNWRAP_CONTINUE(subscription, sub_wptr.lock());
        subscription.m_channel = nullptr;
    }

    subs_lref.get().clear();
}
