#pragma once

#include <mutex>

template <class T>
class Guarded;

template <class T>
class LockedRef
{
private:
    friend class Guarded<T>;

    LockedRef(T & value, std::mutex & mutex)
        : m_value(value)
        , m_guard(mutex)
    {
    }

public:
    T & get() { return m_value; }

private:
    T & m_value;

    std::lock_guard<std::mutex> m_guard;
};

template <class T>
class Guarded
{
public:
    template <class... Args>
    Guarded(Args &&... args)
        : m_value(std::forward<Args>(args)...)
    {
    }

    LockedRef<T> lock() { return LockedRef<T>{m_value, m_mutex}; }

private:
    T m_value;
    std::mutex m_mutex;
};
