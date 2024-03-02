#pragma once

#include "ThreadSafeQueue.h"

template <class T>
class IEventInvoker
{
public:
    virtual void invoke(const T & value) = 0;
};

template <class T>
class EventLoop
{
public:
    EventLoop(IEventInvoker<T> & invoker)
        : m_invoker(invoker)
    {
    }

    ~EventLoop()
    {
        m_queue.stop();
        m_thread.join();
    }

    bool push(const T & value)
    {
        return m_queue.push(value);
    }

private:
    void run()
    {
        while (m_queue.wait_and_pop()) {
            m_invoker.invoke(m_queue.pop());
        }
    }

private:
    IEventInvoker<T> & m_invoker;

    ThreadSafeQueue<T> m_queue;
    std::thread m_thread;
};
