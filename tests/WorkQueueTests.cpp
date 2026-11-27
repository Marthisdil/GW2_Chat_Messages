// Tests for WorkQueue (the background thread that writes files off the render thread).

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "TestCheck.h"
#include "WorkQueue.h"

namespace
{
    // Holds the worker on a job until Open, so jobs can be posted while it's busy.
    struct Gate
    {
        std::mutex              Mutex;
        std::condition_variable Cv;
        bool                    Opened = false;
        std::atomic<bool>       Entered{false};

        void Wait()
        {
            Entered = true;
            std::unique_lock<std::mutex> lock(Mutex);
            Cv.wait(lock, [this] { return Opened; });
        }
        void Open()
        {
            {
                std::lock_guard<std::mutex> lock(Mutex);
                Opened = true;
            }
            Cv.notify_all();
        }
        void WaitEntered() const
        {
            while (!Entered) std::this_thread::yield();
        }
    };
}

static void TestInlineWhenNotStarted()
{
    WorkQueue q;
    const std::thread::id me = std::this_thread::get_id();
    std::thread::id ranOn;
    q.Post([&] { ranOn = std::this_thread::get_id(); });
    CHECK(ranOn == me); // no thread: run at once, here
    q.Flush();          // nothing to wait for
    CHECK(!q.Running());
}

static void TestOrderAndFlush()
{
    WorkQueue q;
    q.Start();
    CHECK(q.Running());
    std::mutex m;
    std::vector<int> order;
    std::thread::id ranOn;
    for (int i = 0; i < 20; i++)
        q.Post([&, i] {
            std::lock_guard<std::mutex> lock(m);
            order.push_back(i);
            ranOn = std::this_thread::get_id();
        });
    q.Flush();
    {
        std::lock_guard<std::mutex> lock(m);
        CHECK(order.size() == 20);
        bool inOrder = true;
        for (int i = 0; i < static_cast<int>(order.size()); i++) inOrder = inOrder && order[static_cast<size_t>(i)] == i;
        CHECK(inOrder);
        CHECK(ranOn != std::this_thread::get_id()); // on the worker
    }
    q.Stop();
    CHECK(!q.Running());
}

static void TestKeyReplacesWaitingJob()
{
    WorkQueue q;
    q.Start();
    Gate gate;
    std::vector<int> ran; // only touched by the worker until Flush returns
    q.Post([&] { gate.Wait(); });
    gate.WaitEntered(); // the worker is busy: the next jobs wait
    q.Post([&] { ran.push_back(1); }, "settings");
    q.Post([&] { ran.push_back(2); });
    q.Post([&] { ran.push_back(3); }, "settings"); // replaces 1, in 1's place
    q.Post([&] { ran.push_back(4); }, "macros");
    gate.Open();
    q.Flush();
    CHECK(ran.size() == 3);
    CHECK(ran.size() == 3 && ran[0] == 3 && ran[1] == 2 && ran[2] == 4);
    // A key that's already running isn't replaced: a new job with it waits its turn.
    Gate gate2;
    std::vector<int> ran2;
    q.Post([&] { gate2.Wait(); ran2.push_back(1); }, "settings");
    gate2.WaitEntered();
    q.Post([&] { ran2.push_back(2); }, "settings");
    gate2.Open();
    q.Flush();
    CHECK(ran2.size() == 2 && ran2[0] == 1 && ran2[1] == 2);
    q.Stop();
}

static void TestExceptionIsContained()
{
    WorkQueue q;
    q.Start();
    bool after = false;
    q.Post([] { throw std::runtime_error("disk full"); });
    q.Post([&] { after = true; });
    q.Flush();
    CHECK(after);
    CHECK(q.Failures() == 1);
    q.Stop();
    // Inline (not started) too.
    q.Post([] { throw 42; }); // NOLINT(bugprone-std-exception-baseclass): tests that a non-std exception is contained too
    CHECK(q.Failures() == 2);
}

static void TestStopRunsWhatsWaiting()
{
    WorkQueue q;
    q.Start();
    Gate gate;
    std::atomic<int> count{0};
    q.Post([&] { gate.Wait(); });
    gate.WaitEntered();
    for (int i = 0; i < 5; i++) q.Post([&] { count++; });
    gate.Open();
    q.Stop(); // nothing is lost at unload
    CHECK(count == 5);
    // After Stop, posting runs at once; Start again works.
    q.Post([&] { count++; });
    CHECK(count == 6);
    q.Start();
    q.Post([&] { count++; });
    q.Flush();
    CHECK(count == 7);
    q.Stop();
}

void RunWorkQueueTests()
{
    TestInlineWhenNotStarted();
    TestOrderAndFlush();
    TestKeyReplacesWaitingJob();
    TestExceptionIsContained();
    TestStopRunsWhatsWaiting();
}
