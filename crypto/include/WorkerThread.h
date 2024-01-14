#pragma once

#include <condition_variable>
#include <functional>
#include <thread>

class WorkerThreadOnce
{
public:
    WorkerThreadOnce(std::function<void()> && fn);
    ~WorkerThreadOnce();

    void stop_async();
    void wait_for_finish();

private:
    std::atomic_bool m_running = false;
    std::atomic_bool m_finished = false;

    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::thread m_thread;
};

class WorkerThreadLoop
{
public:
    WorkerThreadLoop(std::function<bool(const std::atomic_bool &)> && fn);
    ~WorkerThreadLoop();

    void set_on_finish(std::function<void()> && fn);

    void stop_async();
    void wait_for_finish();

private:
    std::atomic_bool m_running = false;
    std::atomic_bool m_finished = false;

    std::function<void()> m_on_finish = [](){};

    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::thread m_thread;
};
