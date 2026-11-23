// Tests for Cooldown (hotkey cooldowns and the 1 s guard).

#include "Cooldown.h"
#include "TestCheck.h"

using namespace Cooldown;

static void TestNoSendYet()
{
    Tracker t;
    const Wait w = t.ForHotkey(1, 30, 100.0);
    CHECK(w.Seconds == 0.0 && !w.ByCooldown);
    CHECK(!t.TakeAlert(1));
}

static void TestGuardOnly()
{
    Tracker t;
    t.Started(1, 10.0);
    t.Ended(1, 12.0, true);
    Wait w = t.ForHotkey(1, 0, 12.5); // no cooldown: only the guard
    CHECK(w.Seconds > 0.49 && w.Seconds < 0.51 && !w.ByCooldown);
    w = t.ForHotkey(1, 0, 13.0);
    CHECK(w.Seconds == 0.0);
    CHECK(t.ForHotkey(1, 0, 20.0).Seconds == 0.0);
    // Other macros aren't affected.
    CHECK(t.ForHotkey(2, 0, 12.5).Seconds == 0.0);
}

static void TestCooldown()
{
    Tracker t;
    t.Started(1, 10.0);
    t.Ended(1, 11.0, true);
    Wait w = t.ForHotkey(1, 5, 11.5); // cooldown from the start: 3.5 s left
    CHECK(w.ByCooldown && w.Seconds > 3.49 && w.Seconds < 3.51);
    w = t.ForHotkey(1, 5, 14.9);
    CHECK(w.ByCooldown && w.Seconds > 0.0);
    CHECK(t.ForHotkey(1, 5, 15.0).Seconds == 0.0);
    // A 1 s cooldown is shorter than the guard after a long send: the guard wins, silently.
    t.Started(1, 20.0);
    t.Ended(1, 22.0, true);
    w = t.ForHotkey(1, 1, 22.2);
    CHECK(!w.ByCooldown && w.Seconds > 0.79 && w.Seconds < 0.81);
    // Out-of-range cooldowns are clamped.
    t.Started(3, 0.0);
    CHECK(t.ForHotkey(3, -5, 1.0).Seconds == 0.0);
    CHECK(t.ForHotkey(3, 100000, 1.0).Seconds > kMaxSec - 2);
}

static void TestFailedSend()
{
    // A failed send starts no cooldown, only the guard.
    Tracker t;
    t.Started(1, 10.0);
    t.Ended(1, 10.5, false);
    Wait w = t.ForHotkey(1, 60, 11.0);
    CHECK(!w.ByCooldown && w.Seconds > 0.49 && w.Seconds < 0.51);
    CHECK(t.ForHotkey(1, 60, 11.6).Seconds == 0.0);
}

static void TestAlertOnce()
{
    Tracker t;
    t.Started(1, 10.0);
    t.Ended(1, 10.5, true);
    CHECK(t.TakeAlert(1));
    CHECK(!t.TakeAlert(1));
    CHECK(!t.TakeAlert(1));
    t.Started(1, 50.0); // a new send: a new cooldown, a new alert
    CHECK(t.TakeAlert(1));
    CHECK(!t.TakeAlert(1));
    t.Forget(1);
    CHECK(t.ForHotkey(1, 60, 50.5).Seconds == 0.0);
}

static void TestWholeSeconds()
{
    CHECK(WholeSeconds(0.0) == 0);
    CHECK(WholeSeconds(-3.0) == 0);
    CHECK(WholeSeconds(0.01) == 1);
    CHECK(WholeSeconds(1.0) == 1);
    CHECK(WholeSeconds(1.2) == 2);
    CHECK(WholeSeconds(59.99) == 60);
    CHECK(WholeSeconds(1e12) == kMaxSec + 1);
}

void RunCooldownTests()
{
    TestNoSendYet();
    TestGuardOnly();
    TestCooldown();
    TestFailedSend();
    TestAlertOnce();
    TestWholeSeconds();
}
