#include "WorkQueue.h"

#include <utility>

void WorkQueue::Start()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Started) return;
    m_Stop    = false;
    m_Started = true;
    m_Thread  = std::thread([this] { Main(); });
}

void WorkQueue::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Started) return;
        m_Stop = true;
    }
    m_Wake.notify_all();
    if (m_Thread.joinable()) m_Thread.join();
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Started = false;
    m_Stop    = false;
}

bool WorkQueue::Running() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Started;
}

void WorkQueue::Post(std::function<void()> aJob, const std::string& aKey)
{
    if (!aJob) return;
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        if (!m_Started)
        {
            lock.unlock();
            RunSafely(aJob); // no thread (yet, or any more): do it now
            return;
        }
        if (!aKey.empty())
        {
            for (Job& j : m_Jobs)
            {
                if (j.Key == aKey)
                {
                    j.Fn = std::move(aJob); // still waiting: the newer data replaces it
                    return;
                }
            }
        }
        m_Jobs.push_back({aKey, std::move(aJob)});
    }
    m_Wake.notify_one();
}

void WorkQueue::Flush()
{
    std::unique_lock<std::mutex> lock(m_Mutex);
    if (!m_Started) return;
    m_Idle.wait(lock, [this] { return m_Jobs.empty() && !m_Busy; });
}

void WorkQueue::RunSafely(const std::function<void()>& aJob)
{
    // An exception escaping a thread would end the game (std::terminate): count it instead.
    try
    {
        aJob();
    }
    catch (...)
    {
        m_Failures.fetch_add(1);
    }
}

void WorkQueue::Main()
{
    for (;;)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Wake.wait(lock, [this] { return m_Stop || !m_Jobs.empty(); });
            if (m_Jobs.empty()) return; // stopping, and nothing is left to run
            job = std::move(m_Jobs.front());
            m_Jobs.pop_front();
            m_Busy = true;
        }
        RunSafely(job.Fn);
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Busy = false;
        }
        m_Idle.notify_all();
    }
}
