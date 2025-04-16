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
    ThreadSafePriorityQueue()
    {
        m_queue_map[Priority::High];
        m_queue_map[Priority::Normal];
        m_queue_map[Priority::Low];
    }

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
        std::queue<T> * queue = nullptr;

        m_cv.wait(lock, [this, &queue] {
            if (!m_keep_waiting) {
                return true;
            }
            for (auto & [p, q] : m_queue_map) {
                if (!q.empty()) {
                    queue = &q;
                    return true;
                }
            }
            return false;
        });

        if (!m_keep_waiting) {
            return std::nullopt;
        }

        T value = std::move(queue->front());
        queue->pop();
        return value;
    }

private:
    std::mutex m_mutex;
    std::map<Priority, std::queue<T>> m_queue_map;
    std::condition_variable m_cv;
    bool m_keep_waiting{true};
};
