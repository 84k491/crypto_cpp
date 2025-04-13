#pragma once

#include "EventChannel.h"
#include "EventLoop.h"
#include "EventTimeseriesChannel.h"
#include "ISubsription.h"
#include "crossguid/guid.hpp"

#include <map>
#include <memory>

template <class... Args>
class EventInvokerDispatcher;

template <class... Args>
class InvokerSubscription : public ISubscription
{
    friend EventInvokerDispatcher<Args...>;

public:
    template <class EventT>
    using CallbackT = std::function<void(const EventT &)>;
    using GenericCallbackT = std::function<void(const std::variant<Args...> &)>;

    template <class EventT>
    InvokerSubscription(EventInvokerDispatcher<Args...> & invoker, CallbackT<EventT> callback)
        : m_guid(xg::newGuid())
        , m_callback([callback](const std::variant<Args...> & value) {
            if (!std::holds_alternative<EventT>(value)) {
                return;
            }
            callback(std::get<EventT>(value));
        })
        , m_invoker(&invoker)
    {
        static_assert((std::is_same_v<EventT, Args> || ...), "Event type must be one of event loop types");
    };

    ~InvokerSubscription() override
    {
        if (m_invoker) {
            m_invoker->unsubscribe(m_guid);
        }
    }

    void try_invoke(const std::variant<Args...> & value)
    {
        std::visit(m_callback, value);
    }

    xg::Guid m_guid;

private:
    GenericCallbackT m_callback;
    EventInvokerDispatcher<Args...> * m_invoker = nullptr; // TODO smart pointer?
};

template <class... Args>
class EventInvokerDispatcher : public IEventInvoker<Args...>
{
public:
    ~EventInvokerDispatcher()
    {
        auto lref = m_subscriptions.lock();
        for (auto & sub : lref.get()) {
            sub->m_invoker = nullptr;
        }
        lref.get().clear();
    }

    void invoke(const std::variant<Args...> & value) override
    {
        auto lref = m_subscriptions.lock();
        for (auto & sub : lref.get()) {
            sub->try_invoke(value);
        }
    }

    template <class EventT>
    [[nodiscard("Save RAII sub")]] std::shared_ptr<ISubscription> register_invoker(std::function<void(const EventT &)> cb)
    {
        auto sub = std::make_shared<InvokerSubscription<Args...>>(*this, cb);
        auto lref = m_subscriptions.lock();
        lref.get().push_back(sub);
        return sub;
    }

    void unsubscribe(xg::Guid guid)
    {
        auto lref = m_subscriptions.lock();
        for (auto it = lref.get().begin(); it != lref.get().end(); ++it) {
            if ((*it)->m_guid == guid) {
                lref.get().erase(it);
                break;
            }
        }
    }

private:
    Guarded<std::list<std::shared_ptr<InvokerSubscription<Args...>>>> m_subscriptions{};
};

// Represents an event loop with all it's subscriptions
// Needed to guarantee that event loop will unsubscribe from all channels before destruction
template <class... Args>
class EventLoopSubscriber
{
public:
    EventLoopSubscriber()
        : m_event_loop(EventLoop<Args...>::create(&m_invoker_dispatcher))
    {
    }

    template <class EventT>
    void push_event(EventT ev)
    {
        static_cast<IEventConsumer<EventT> &>(*m_event_loop).push(ev);
    }

    template <class EventT>
    void push_delayed(std::chrono::milliseconds t, EventT ev)
    {
        static_cast<IEventConsumer<EventT> &>(*m_event_loop).push_delayed(t, ev);
    }

    template <class EventChannelT>
    void subscribe(EventChannelT & channel, const std::string & bucket_name = "")
    {
        const auto sub = channel.subscribe(m_event_loop);
        m_subscriptions[bucket_name].push_back(sub);
    }

    template <class EventChannelT, class UpdateCallbackT>
    void subscribe(EventChannelT & channel, UpdateCallbackT && callback, const std::string & bucket_name = "")
    {
        const auto sub = channel.subscribe(m_event_loop, std::forward<UpdateCallbackT>(callback));
        m_subscriptions[bucket_name].push_back(sub);
    }

    void unsubscribe(const std::string & bucket_name)
    {
        m_subscriptions.erase(bucket_name);
    }

    void unsubscribe_all()
    {
        m_subscriptions.clear();
    }

    auto & invoker()
    {
        return m_invoker_dispatcher;
    }

private:
    EventInvokerDispatcher<Args...> m_invoker_dispatcher;
    std::shared_ptr<EventLoop<Args...>> m_event_loop;
    std::map<std::string, std::list<std::shared_ptr<ISubscription>>> m_subscriptions; // those must be destroyed before EvLoop
};
