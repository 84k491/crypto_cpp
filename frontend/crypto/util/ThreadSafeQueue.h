#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue() = default;

    ~ThreadSafeQueue()
    {
        stop();
    }

    void stop()
    {
        std::lock_guard l(m_mutex);
        m_keep_waiting = false;
        m_cv.notify_all();
    }

    bool push(const T & value)
    {
        if (!m_keep_waiting) {
            return false;
        }
        std::lock_guard lock(m_mutex);
        m_queue.push(value);
        m_cv.notify_one();
        return true;
    }

    std::optional<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_keep_waiting; });
        if (!m_keep_waiting) {
            return std::nullopt;
        }

        T value = std::move(m_queue.front());
        m_queue.pop();
        return value;
    }

private:
    std::mutex m_mutex;
    std::queue<T> m_queue;
    std::condition_variable m_cv;
    bool m_keep_waiting{true};
};
