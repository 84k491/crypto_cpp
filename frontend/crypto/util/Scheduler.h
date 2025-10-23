#pragma once

#include "Events.h"
#include "EventChannel.h"

#include <condition_variable>
#include <functional>
#include <map>
#include <thread>

class Scheduler
{
    using CallbackT = std::function<void()>;

    Scheduler()
    {
        m_thread = std::thread([this] { run(); });
    }

    struct IdentifiedLambdaEvent
    {
        xg::Guid target_el_id;
        LambdaEvent ev;
    };

public:
    static Scheduler & i()
    {
        static Scheduler s{};
        return s;
    }

    ~Scheduler()
    {
        m_running = false;
        m_cv.notify_all();
        m_thread.join();
    }

    void delay_event(xg::Guid el_guid, std::chrono::milliseconds delay, LambdaEvent event)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto future_ts = now + delay;

        std::unique_lock<std::mutex> lock(m_mutex);

        m_delayed_events.emplace(future_ts, IdentifiedLambdaEvent{.target_el_id = el_guid, .ev = std::move(event)});

        m_cv.notify_all();
    }

    EventChannel<LambdaEvent> & delayed_channel(xg::Guid guid)
    {
        return m_channels[guid];
    }

private:
    void run()
    {
        while (m_running) {
            const auto now = std::chrono::steady_clock::now();
            std::unique_lock<std::mutex> lock(m_mutex);

            for (auto it = m_delayed_events.begin(), end = m_delayed_events.end(); it != end;) {
                if (it->first <= now) {
                    auto & [guid, ev] = it->second;

                    const auto ch_it = m_channels.find(guid);
                    if (ch_it == m_channels.end()) {
                        throw std::runtime_error("no channel in scheduler");
                    }
                    auto & ch = ch_it->second;

                    ch.push(std::move(ev));
                    it = m_delayed_events.erase(it);
                }
                else {
                    ++it;
                }
            }

            if (m_delayed_events.empty()) {
                m_cv.wait(lock, [this] { return !m_running || !m_delayed_events.empty(); });
            }
            else {
                m_cv.wait_until(
                        lock,
                        m_delayed_events.begin()->first,
                        [this] {
                            return !m_running ||
                                    m_delayed_events.begin()->first <= std::chrono::steady_clock::now();
                        });
            }
        }
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::multimap<std::chrono::time_point<std::chrono::steady_clock>, IdentifiedLambdaEvent> m_delayed_events;

    std::atomic_bool m_running = true;
    std::thread m_thread;

    std::map<xg::Guid, EventChannel<LambdaEvent>> m_channels;
};
