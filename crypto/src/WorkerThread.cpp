#include "WorkerThread.h"

WorkerThread::WorkerThread(std::function<bool()> && fn)
    : m_thread([&, fn = std::move(fn)]() {
        {
            std::lock_guard l{m_mutex};
            m_running = true;
        }

        while (m_running && fn()) {
        }

        std::lock_guard l{m_mutex};
        m_finished = true;
        m_cv.notify_all();
    }){};

void WorkerThread::stop_async()
{
    m_running = false;
}

void WorkerThread::wait_for_finish()
{
    std::unique_lock lock{m_mutex};
    m_cv.wait(lock, [this] { return m_finished.load(); });
}

WorkerThread::~WorkerThread()
{
    m_running = false;
    stop_async();
    m_thread.join();
}
