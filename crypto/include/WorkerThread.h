#pragma once

#include <condition_variable>
#include <functional>
#include <thread>

class WorkerThread
{
public:
    WorkerThread(std::function<bool()> && fn);
    ~WorkerThread();

    void stop_async();
    void wait_for_finish();

private:
    std::atomic_bool m_running = false;

    std::atomic_bool m_finished = false;
    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::thread m_thread;
};
