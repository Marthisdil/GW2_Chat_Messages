#include "Hotkeys.h"

#include <windows.h>

#include <atomic>
#include <deque>
#include <mutex>

#include "Globals.h"
#include "Mumble.h"

namespace
{
    constexpr size_t kMaxQueued = 8; // presses waiting for the render thread; extra ones are dropped

    Mumble::Data*    s_Mumble = nullptr;
    NexusLinkData_t* s_Nexus  = nullptr;

    std::atomic<bool> s_UiTyping{false};

    std::mutex                s_QueueMutex;
    std::deque<Hotkey::Combo> s_Queue;

    // Capture: 0 idle, 1 waiting for a key, 2 got one (s_Captured), 3 cancelled.
    std::atomic<int>      s_CaptureState{0};
    std::atomic<uint32_t> s_Captured{0};

    bool Down(int aVk) { return (GetKeyState(aVk) & 0x8000) != 0; } // state as of this message

    uint16_t KeyOf(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (lParam & (LPARAM(1) << 30)) return 0; // auto-repeat while held
            return static_cast<uint16_t>(wParam);
        case WM_MBUTTONDOWN:
            return Hotkey::kVkMButton;
        case WM_XBUTTONDOWN:
            return GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? Hotkey::kVkXButton1 : Hotkey::kVkXButton2;
        default:
            return 0;
        }
    }

    UINT OnWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        (void)hWnd;
        const uint16_t key = KeyOf(uMsg, wParam, lParam);
        if (key == 0 || Hotkey::IsModifierKey(key)) return uMsg;

        Hotkey::Combo c;
        c.Key   = key;
        c.Ctrl  = Down(VK_CONTROL);
        c.Alt   = Down(VK_MENU);
        c.Shift = Down(VK_SHIFT);

        if (s_CaptureState.load() == 1)
        {
            if (key == Hotkey::kVkEscape && !c.HasModifier()) s_CaptureState = 3;
            else
            {
                s_Captured = c.Pack();
                s_CaptureState = 2;
            }
            return uMsg; // still passed on: we never block input
        }

        if (s_UiTyping) return uMsg;                                        // typing in an addon window
        if (s_Mumble && s_Mumble->Context.IsTextboxFocused) return uMsg;    // typing in game
        if (!s_Nexus || !s_Nexus->IsGameplay) return uMsg;                  // loading screen, character select

        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            if (s_Queue.size() < kMaxQueued) s_Queue.push_back(c);
        }
        return uMsg; // never consumed
    }
}

void Hotkeys::Init()
{
    s_Mumble = static_cast<Mumble::Data*>(g_Api->DataLink_Get(DL_MUMBLE_LINK));
    s_Nexus  = static_cast<NexusLinkData_t*>(g_Api->DataLink_Get(DL_NEXUS_LINK));
    s_CaptureState = 0;
    {
        std::lock_guard<std::mutex> lock(s_QueueMutex);
        s_Queue.clear();
    }
    g_Api->WndProc_Register(OnWndProc);
}

void Hotkeys::Shutdown()
{
    g_Api->WndProc_Deregister(OnWndProc);
    s_CaptureState = 0;
    {
        std::lock_guard<std::mutex> lock(s_QueueMutex);
        s_Queue.clear();
    }
    s_Mumble = nullptr;
    s_Nexus  = nullptr;
}

void Hotkeys::SetUiTyping(bool aTyping) { s_UiTyping = aTyping; }

bool Hotkeys::PopPress(Hotkey::Combo& aOut)
{
    std::lock_guard<std::mutex> lock(s_QueueMutex);
    if (s_Queue.empty()) return false;
    aOut = s_Queue.front();
    s_Queue.pop_front();
    return true;
}

void Hotkeys::StartCapture() { s_CaptureState = 1; }
void Hotkeys::CancelCapture() { s_CaptureState = 0; }
bool Hotkeys::IsCapturing() { return s_CaptureState.load() == 1; }

bool Hotkeys::PopCapture(Hotkey::Combo& aOut, bool& aCancelled)
{
    int state = s_CaptureState.load();
    if (state != 2 && state != 3) return false;
    if (!s_CaptureState.compare_exchange_strong(state, 0)) return false;
    aCancelled = state == 3;
    aOut = Hotkey::Combo::Unpack(s_Captured.load());
    return true;
}
