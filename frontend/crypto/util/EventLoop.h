#pragma once

#include "ThreadSafeQueue.h"

#include <iostream>

template <class T>
class IEventInvoker
{
public:
    virtual ~IEventInvoker() = default;
    virtual void invoke(const T & value) = 0;
};

template <class T>
class IEventConsumer
{
public:
    virtual ~IEventConsumer() = default;
    virtual bool push(const T & value) = 0;
};

template <class T>
class EventLoop : public IEventConsumer<T>
{
public:
    EventLoop(IEventInvoker<T> & invoker)
        : m_invoker(invoker)
    {
        m_thread = std::thread([this] { run(); });
    }

    ~EventLoop()
    {
        m_queue.stop();
        m_thread.join();
    }

    bool push(const T & value) override
    {
        std::cout << "Pushing in event loop" << std::endl;
        return m_queue.push(value);
    }

private:
    void run()
    {
        while (true) {
            const auto opt = m_queue.wait_and_pop();
            if (!opt) {
                return;
            }
            m_invoker.invoke(opt.value());
        }
    }

private:
    IEventInvoker<T> & m_invoker;

    ThreadSafeQueue<T> m_queue;
    std::thread m_thread;
};
