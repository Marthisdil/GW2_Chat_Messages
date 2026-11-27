#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

// One background thread that runs jobs in the order they were posted, so file writes never hold
// up a frame (docs/design.md, "Saving off the render thread"). No Nexus or ImGui dependency.
//
// - A job posted with a key replaces the job with that key while it's still waiting (it keeps
//   its place in the line): a burst of saves of one file writes it once, with the newest data.
// - Before Start (and after Stop) Post runs the job at once, on the caller's thread.
// - A job that throws is counted (Failures) and the next one runs: nothing leaves the thread.
// - Stop runs every job that's still waiting, then ends the thread.
// - Flush waits until every job posted so far has run. Never call Flush or Stop from a job.
class WorkQueue
{
public:
    WorkQueue() = default;
    WorkQueue(const WorkQueue&) = delete;
    WorkQueue& operator=(const WorkQueue&) = delete;
    ~WorkQueue() { Stop(); }

    void Start();
    void Stop();
    void Post(std::function<void()> aJob, const std::string& aKey = std::string());
    void Flush();
    bool Running() const;
    size_t Failures() const { return m_Failures.load(); }

private:
    struct Job
    {
        std::string           Key;
        std::function<void()> Fn;
    };

    void Main();
    void RunSafely(const std::function<void()>& aJob);

    mutable std::mutex      m_Mutex;
    std::condition_variable m_Wake; // a job was posted, or Stop
    std::condition_variable m_Idle; // a job finished
    std::deque<Job>         m_Jobs;
    bool                    m_Started = false;
    bool                    m_Stop    = false;
    bool                    m_Busy    = false; // a job is running
    std::thread             m_Thread;
    std::atomic<size_t>     m_Failures{0};
};
