#pragma once

#include "Events.h"
#include "ThreadSafePriorityQueue.h"

#include <functional>
#include <map>
#include <thread>

template <class... Ts>
struct VariantMatcher : Ts...
{
    using Ts::operator()...;
};

class ILambdaAcceptor
{
public:
    virtual ~ILambdaAcceptor() = default;

    bool push(const LambdaEvent value)
    {
        return push_to_queue(std::move(value));
    }

    bool push_delayed(std::chrono::milliseconds delay, const LambdaEvent value)
    {
        return push_to_queue_delayed(delay, std::move(value));
    };

private:
    virtual bool push_to_queue(LambdaEvent value) = 0;
    virtual bool push_to_queue_delayed(std::chrono::milliseconds delay, LambdaEvent value) = 0;
};

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

class EventLoopSubscriber;

class EventLoop : public std::enable_shared_from_this<EventLoop>
    , public ILambdaAcceptor
{
    EventLoop()
    {
        m_thread = std::thread([this] { run(); });
    }

    static auto create()
    {
        return std::shared_ptr<EventLoop>(new EventLoop());
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
    ILambdaAcceptor & as_consumer()
    {
        return static_cast<ILambdaAcceptor &>(*this);
    }

protected:
    bool push_to_queue(LambdaEvent value) override
    {
        return m_queue.push(std::move(value));
    }

    bool push_to_queue_delayed(std::chrono::milliseconds delay, LambdaEvent value) override
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
    friend class EventLoopSubscriber;

    void run()
    {
        while (true) {
            const auto opt = m_queue.wait_and_pop();
            if (!opt) {
                return;
            }

            opt.value().func();
        }
    }

private:
    ThreadSafePriorityQueue<LambdaEvent> m_queue{};
    std::thread m_thread;
};
