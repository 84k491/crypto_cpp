#pragma once

#include "Priority.h"

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <list>

template <typename T>
class ThreadSafePriorityQueue
{
public:
    ThreadSafePriorityQueue()
    {
        m_queue_map[Priority::High];
        m_queue_map[Priority::Normal];
        m_queue_map[Priority::Low];
        m_queue_map[Priority::Barrier];
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

    void discard_events(std::function<bool(const T &)> && pred)
    {
        std::lock_guard lock(m_mutex);

        for (auto & [_, q] : m_queue_map) {
            for (auto it = q.begin(), end = q.end(); it != end; /*nothing*/) {
                if (pred(*it)) {
                    it = q.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
    }

    bool push(const T & value)
    {
        if (!m_keep_waiting) {
            return false;
        }

        const auto priority = value.priority();

        std::lock_guard lock(m_mutex);

        auto & queue = m_queue_map[priority];
        queue.push_back(value);

        m_cv.notify_one();

        return true;
    }

    std::optional<T> wait_and_pop()
    {
        std::unique_lock lock(m_mutex);
        std::list<T> * queue = nullptr;

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
        queue->pop_front();

        return value;
    }

private:
    std::mutex m_mutex;
    std::map<Priority, std::list<T>> m_queue_map;
    std::condition_variable m_cv;
    bool m_keep_waiting{true};
};
