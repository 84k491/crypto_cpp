#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <variant>

enum class Priority
{
    High = 0,
    Normal = 1,
    Low = 2,
};

template <typename T>
class ThreadSafePriorityQueue
{
public:
    ThreadSafePriorityQueue() = default;

    ~ThreadSafePriorityQueue()
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
        const auto priority = std::visit([](auto && v) -> Priority { return v.priority(); }, value);
        std::lock_guard lock(m_mutex);
        auto & queue = m_queue_map[priority];
        queue.push(value);
        m_cv.notify_one();
        return true;
    }

    std::optional<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue_map.empty() || !m_keep_waiting; });
        if (!m_keep_waiting) {
            return std::nullopt;
        }

        auto & queue = m_queue_map.begin()->second;
        T value = std::move(queue.front());
        queue.pop();
        if (queue.empty()) {
            m_queue_map.erase(m_queue_map.begin()); // TODO double free here
        }
        return value;
    }

private:
    std::mutex m_mutex;
    std::map<Priority, std::queue<T>> m_queue_map;
    std::condition_variable m_cv;
    bool m_keep_waiting{true};
};
