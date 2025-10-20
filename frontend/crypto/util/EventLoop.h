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

    virtual void push(LambdaEvent value) = 0;
    virtual void push_delayed(std::chrono::milliseconds delay, LambdaEvent value) = 0;
};

class EventLoopWithDelays;
class EventLoop : public ILambdaAcceptor
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

protected:
    void push(LambdaEvent value) override
    {
        m_queue.push(std::move(value));
    }

    void push_delayed(std::chrono::milliseconds, LambdaEvent) override
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
