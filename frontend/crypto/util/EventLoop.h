#pragma once

#include "EventLoopSubscriber.h"
#include "Events.h"
#include "ILambdaAcceptor.h"
#include "Scheduler.h"
#include "ThreadSafePriorityQueue.h"

#include <functional>
#include <thread>

template <class... Ts>
struct VariantMatcher : Ts...
{
    using Ts::operator()...;
};

class EventLoop;
class BasicEventLoop : public ILambdaAcceptor
{
    friend class EventLoop;

public:
    BasicEventLoop()
    {
        m_thread = std::thread([this] { run(); });
    }

    ~BasicEventLoop() override
    {
        stop();
    }

    void stop()
    {
        m_queue.stop();
        m_thread.join();
    }

    void discard_subscriber_events(xg::Guid sub_guid) override
    {
        m_queue.discard_events(
                [sub_guid](const LambdaEvent & ev) -> bool {
                    return ev.m_subscriber_guid == sub_guid;
                });
    }

protected:
    void push(LambdaEvent value) override
    {
        m_queue.push(std::move(value));
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

class EventLoop final : public ILambdaAcceptor
{
public:
    EventLoop()
        : m_sub(m_ev)
    {
        auto & ch = Scheduler::i().delayed_channel(m_guid);
        m_sub.subscribe(
                ch,
                [this](LambdaEvent ev) {
                    m_ev.push(std::move(ev));
                });
    }

    void push(LambdaEvent event) override
    {
        return m_ev.push(std::move(event));
    }

    void push_delayed(std::chrono::milliseconds delay, LambdaEvent value) override
    {
        Scheduler::i().delay_event(m_guid, delay, value);
    }

    void discard_subscriber_events(xg::Guid sub_guid) override
    {
        m_ev.discard_subscriber_events(sub_guid);
    }

private:
    xg::Guid m_guid;

    BasicEventLoop m_ev;
    EventSubcriber m_sub;
};
