#include "WorkerThread.h"

WorkerThreadOnce::WorkerThreadOnce(std::function<void()> && fn)
    : m_thread([this, fn = std::move(fn)]() {
        {
            std::lock_guard l{m_mutex};
            m_running = true;
        }

        fn();

        std::lock_guard l{m_mutex};
        m_finished = true;
        m_cv.notify_all();
    }){};

void WorkerThreadOnce::stop_async()
{
    m_running = false;
}

void WorkerThreadOnce::wait_for_finish()
{
    std::unique_lock lock{m_mutex};
    m_cv.wait(lock, [this] { return m_finished.load(); });
}

WorkerThreadOnce::~WorkerThreadOnce()
{
    m_running = false;
    stop_async();
    m_thread.join();
}

WorkerThreadLoop::WorkerThreadLoop(std::function<bool(const std::atomic_bool &)> && fn)
    : m_thread([this, fn = std::move(fn)]() {
        {
            std::lock_guard l{m_mutex};
            m_running = true;
        }

        while (m_running && fn(m_running)) {
        }

        std::lock_guard l{m_mutex};
        m_finished = true;
        m_on_finish();
        m_cv.notify_all();
    }){};

void WorkerThreadLoop::set_on_finish(std::function<void()> && fn)
{
    m_on_finish = std::move(fn);
}

void WorkerThreadLoop::stop_async()
{
    m_running = false;
}

void WorkerThreadLoop::wait_for_finish()
{
    std::unique_lock lock{m_mutex};
    m_cv.wait(lock, [this] { return m_finished.load(); });
}

WorkerThreadLoop::~WorkerThreadLoop()
{
    m_running = false;
    stop_async();
    m_thread.join();
}
