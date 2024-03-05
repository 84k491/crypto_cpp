#pragma once

#include "ThreadSafeQueue.h"

#include <any>
#include <iostream>
#include <variant>

template <class... Args>
class IEventInvoker
{
public:
    virtual ~IEventInvoker() = default;

    virtual void invoke(const std::variant<Args...> & value) = 0;
};

template <
        class T,
        std::enable_if_t<std::is_polymorphic_v<T>, bool> = true> // event type must be polymorphic to use typeid
class IEventConsumer
{
public:
    virtual ~IEventConsumer() = default;

    bool push(const T value)
    {
        return push_to_queue(std::move(value));
    }

private:
    virtual bool push_to_queue(const std::any value) = 0;
};

template <class... Args>
auto any_to_variant_cast(std::any a) -> std::variant<Args...>
{
    if (!a.has_value()) {
        throw std::bad_any_cast();
    }

    std::optional<std::variant<Args...>> v = std::nullopt;

    // TODO use event with a virtual function
    bool found = ((a.type() == typeid(Args) && (v = std::any_cast<Args>(std::move(a)), true)) || ...);

    if (!found) {
        throw std::bad_any_cast{};
    }

    return std::move(*v);
}

template <class... Args>
class EventLoop : public IEventConsumer<Args>...
{
public:
    EventLoop(IEventInvoker<Args...> & invoker)
        : m_invoker(invoker)
    {
        m_thread = std::thread([this] { run(); });
    }

    ~EventLoop() override
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
        std::cout << "Pushing in event loop (any)" << std::endl;
        auto var = any_to_variant_cast<Args...>(value);
        return m_queue.push(std::move(var));
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
    IEventInvoker<Args...> & m_invoker;

    ThreadSafeQueue<std::variant<Args...>> m_queue{};
    std::thread m_thread;
};
