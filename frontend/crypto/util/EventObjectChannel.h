#pragma once

#include "EventLoop.h"
#include "Events.h"
#include "Guarded.h"
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
            ILambdaAcceptor & consumer,
            std::function<void(const ObjectT &)> update_callback,
            EventObjectChannel<ObjectT> & channel,
            xg::Guid guid)
        : m_consumer(consumer)
        , m_callback(update_callback)
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
    ILambdaAcceptor & m_consumer;

    std::function<void(const ObjectT &)> m_callback;
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

    // TODO rename to get_copy
    const ObjectT get()
    {
        auto lref = m_data.lock();
        auto & data = lref.get();
        return data;
    }

    void set_on_sub_count_changed(std::function<void(size_t)> && cb)
    {
        m_sub_count_callback = std::move(cb);
    }

    [[nodiscard]] std::shared_ptr<EventObjectSubscription<ObjectT>>
    subscribe(
            ILambdaAcceptor & consumer,
            std::function<void(const ObjectT &)> && update_callback,
            Priority priority = Priority::Normal);
    void unsubscribe(xg::Guid guid);

    size_t subscribers_count()
    {
        return m_subscriptions.lock().get().size();
    };

private:
    // can't copy because of mutexes
    Guarded<ObjectT> m_data;

    using SubWPtr = std::weak_ptr<EventObjectSubscription<ObjectT>>;

    // Guid is here because wptr will be null on unsub on dtor
    Guarded<std::vector<std::pair<xg::Guid, SubWPtr>>> m_subscriptions;

    std::function<void(size_t)> m_sub_count_callback;
};

template <typename ObjectT>
void EventObjectChannel<ObjectT>::push(const ObjectT & object)
{
    auto subs_lref = m_subscriptions.lock();
    auto data_lref = m_data.lock();

    data_lref.get() = object;

    for (const auto & [_, wptr] : subs_lref.get()) {
        UNWRAP_CONTINUE(subscription, wptr.lock());
        subscription.m_consumer.push(LambdaEvent{
                [cb = subscription.m_callback, object] {
                    cb(object);
                },
                Priority::Normal});
    }
}

template <typename ObjectT>
void EventObjectChannel<ObjectT>::update(std::function<void(ObjectT &)> && update_callback)
{
    auto subs_lref = m_subscriptions.lock();
    auto data_lref = m_data.lock();

    update_callback(data_lref.get());

    // TODO erase if nullptr
    for (const auto & [_, wptr] : subs_lref.get()) {
        UNWRAP_CONTINUE(subscription, wptr.lock());
        subscription.m_consumer.push(LambdaEvent{
                [cb = subscription.m_callback,
                 object = data_lref.get()] {
                    cb(object);
                },
                Priority::Normal});
    }
}

template <typename ObjectT>
std::shared_ptr<EventObjectSubscription<ObjectT>>
EventObjectChannel<ObjectT>::subscribe(
        ILambdaAcceptor & consumer,
        std::function<void(const ObjectT &)> && update_callback,
        Priority) // TODO use priority?
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<EventObjectSubscription<ObjectT>>(consumer, update_callback, *this, guid);

    auto lref = m_subscriptions.lock();

    lref.get().push_back({guid, std::weak_ptr{sptr}});

    if (m_sub_count_callback) {
        m_sub_count_callback(lref.get().size());
    }

    return sptr;
}

template <typename ObjectT>
void EventObjectChannel<ObjectT>::unsubscribe(xg::Guid guid)
{
    size_t sub_count = 0;
    {
        auto lref = m_subscriptions.lock();
        auto & subs = lref.get();

        for (auto it = subs.begin(); it != subs.end(); ++it) {
            auto & [sub_guid, _] = *it;
            if (sub_guid == guid) {
                subs.erase(it);
                break;
            }
        }

        sub_count = subs.size();
    }

    if (m_sub_count_callback) {
        m_sub_count_callback(sub_count);
    }
}

template <typename ObjectT>
EventObjectChannel<ObjectT>::~EventObjectChannel()
{
    auto lref = m_subscriptions.lock();
    auto & subs = lref.get();

    for (const auto [guid, wptr] : subs) {
        auto sptr = wptr.lock();
        if (sptr) {
            sptr->m_channel = nullptr;
        }
    }

    subs.clear();
}
