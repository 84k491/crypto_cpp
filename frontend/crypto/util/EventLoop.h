#pragma once

#include "Events.h"
#include "ThreadSafePriorityQueue.h"

#include <functional>
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

    bool push(LambdaEvent value)
    {
        return push_to_queue(std::move(value));
    }

    bool push_delayed(std::chrono::milliseconds delay, LambdaEvent value)
    {
        return push_to_queue_delayed(delay, std::move(value));
    };

private:
    virtual bool push_to_queue(LambdaEvent value) = 0;
    virtual bool push_to_queue_delayed(std::chrono::milliseconds delay, LambdaEvent value) = 0;
};

class EventLoopWithDelays;
class EventLoop : public std::enable_shared_from_this<EventLoop>
    , public ILambdaAcceptor
{
    friend class EventLoopWithDelays;

public:
    EventLoop()
    {
        m_thread = std::thread([this] { run(); });
    }

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

    bool push_to_queue_delayed(std::chrono::milliseconds, LambdaEvent) override
    {
        throw std::runtime_error("not implemented");
    }

private:
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
    ThreadSafePriorityQueue<LambdaEvent> m_queue;
    std::thread m_thread;
};
