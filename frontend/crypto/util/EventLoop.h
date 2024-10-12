#pragma once

#include "ThreadSafePriorityQueue.h"

#include <any>
#include <functional>
#include <map>
#include <variant>

template <class... Ts>
struct VariantMatcher : Ts...
{
    using Ts::operator()...;
};

template <class... Args>
class IEventInvoker
{
public:
    virtual ~IEventInvoker() = default;

    virtual void invoke(const std::variant<Args...> & value) = 0;
};

template <class EventT>
class IEventConsumer
{
    static_assert(std::is_polymorphic_v<EventT>, "Event type must be polymorphic to use typeid");

public:
    virtual ~IEventConsumer() = default;

    bool push(const EventT value)
    {
        return push_to_queue(std::move(value));
    }

    bool push_delayed(std::chrono::milliseconds delay, const EventT value)
    {
        return push_to_queue_delayed(delay, std::move(value));
    };

private:
    // TODO remove constness
    virtual bool push_to_queue(const std::any value) = 0;
    virtual bool push_to_queue_delayed(std::chrono::milliseconds delay, const std::any value) = 0;
};

template <class... Args>
auto any_to_variant_cast(std::any a) -> std::variant<Args...>
{
    if (!a.has_value()) {
        throw std::bad_any_cast();
    }

    std::optional<std::variant<Args...>> v = std::nullopt;

    bool found = ((a.type() == typeid(Args) && (v = std::any_cast<Args>(std::move(a)), true)) || ...);

    if (!found) {
        throw std::bad_any_cast{};
    }

    return std::move(*v);
}

class Scheduler
{
    using CallbackT = std::function<void()>;

    Scheduler()
    {
        m_thread = std::thread([this] { run(); });
    }

public:
    static Scheduler & i()
    {
        static Scheduler s{};
        return s;
    }

    ~Scheduler()
    {
        m_running = false;
        m_cv.notify_all();
        m_thread.join();
    }

    void delay(std::chrono::milliseconds delay, CallbackT && callback)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto future_ts = now + delay;
        std::unique_lock<std::mutex> lock(m_mutex);
        m_delayed_events.emplace(future_ts, std::move(callback));
        m_cv.notify_all();
    }

private:
    void run()
    {
        while (m_running) {
            const auto now = std::chrono::steady_clock::now();
            std::unique_lock<std::mutex> lock(m_mutex);
            for (auto it = m_delayed_events.begin(), end = m_delayed_events.end(); it != end;) {
                if (it->first <= now) {
                    it->second();
                    it = m_delayed_events.erase(it);
                }
                else {
                    ++it;
                }
            }

            if (m_delayed_events.empty()) {
                m_cv.wait(lock, [this] { return !m_running || !m_delayed_events.empty(); });
            }
            else {
                m_cv.wait_until(
                        lock,
                        m_delayed_events.begin()->first,
                        [this] {
                            return !m_running ||
                                    m_delayed_events.begin()->first <= std::chrono::steady_clock::now();
                        });
            }
        }
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::multimap<std::chrono::time_point<std::chrono::steady_clock>, CallbackT> m_delayed_events;

    std::atomic_bool m_running = true;
    std::thread m_thread;
};

template <class... Args>
class EventLoopHolder;

template <class... Args>
class EventLoop : public std::enable_shared_from_this<EventLoop<Args...>>
    , public IEventConsumer<Args>...
{
    EventLoop(IEventInvoker<Args...> * invoker)
        : m_invoker(invoker)
    {
        m_thread = std::thread([this] { run(); });
    }

    static auto create(IEventInvoker<Args...> * invoker)
    {
        return std::shared_ptr<EventLoop<Args...>>(new EventLoop<Args...>(invoker));
    }

public:
    ~EventLoop() override
    {
        stop();
    }

    void stop()
    {
        m_queue.stop();
        m_thread.join();
    }

    template <class T>
    IEventConsumer<T> & as_consumer()
    {
        return static_cast<IEventConsumer<T> &>(*this);
    }

protected:
    bool push_to_queue(std::any value) override
    {
        auto var = any_to_variant_cast<Args...>(value);
        return m_queue.push(std::move(var));
    }

    bool push_to_queue_delayed(std::chrono::milliseconds delay, const std::any value) override
    {
        Scheduler::i().delay(
                delay,
                [wptr = std::weak_ptr(this->shared_from_this()),
                 value] {
                    auto sptr = wptr.lock();
                    sptr->push_to_queue(value);
                });
        return true;
    }

private:
    friend class EventLoopHolder<Args...>;
    void reset_invoker()
    {
        m_invoker.store(nullptr);
    }

    void run()
    {
        while (true) {
            const auto opt = m_queue.wait_and_pop();
            if (!opt) {
                return;
            }

            if (auto * p = m_invoker.load()) {
                p->invoke(opt.value());
            }
            else {
                return;
            }
        }
    }

private:
    std::atomic<IEventInvoker<Args...> *> m_invoker;

    ThreadSafePriorityQueue<std::variant<Args...>> m_queue{};
    std::thread m_thread;
};

template <class... Args>
class EventLoopHolder
{
public:
    EventLoopHolder(IEventInvoker<Args...> & invoker)
        : m_event_loop(EventLoop<Args...>::create(&invoker))
    {
    }

    EventLoopHolder(const EventLoopHolder<Args...> &) = delete;
    EventLoopHolder(EventLoopHolder<Args...> &&) = delete;

    ~EventLoopHolder() { m_event_loop->reset_invoker(); }

    auto sptr() const
    {
        return m_event_loop;
    }

    EventLoop<Args...> & operator*()
    {
        return *m_event_loop;
    }

    EventLoop<Args...> * operator->()
    {
        return m_event_loop.get();
    }

private:
    std::shared_ptr<EventLoop<Args...>> m_event_loop;
};
