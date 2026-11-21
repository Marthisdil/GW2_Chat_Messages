#include "Sender.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>

#include "Globals.h"
#include "Http.h"
#include "MapNames.h"
#include "Mumble.h"
#include "Placeholders.h"
#include "SendPlan.h"
#include "Storage.h"

namespace
{
    using Clock = std::chrono::steady_clock;
    using SendPlan::EStep;

    constexpr int kChatOpenTimeoutMs   = 750;
    constexpr int kChatCloseTimeoutMs  = 750;
    constexpr int kModifierWaitMs      = 500;
    constexpr int kWindowCallTimeoutMs = 1000;
    constexpr int kBindHoldMs          = 30;
    constexpr int kPollMs              = 10;

    // --- Game state ----------------------------------------------------------------------------

    Mumble::Data*     s_Mumble   = nullptr;
    Mumble::Identity* s_Identity = nullptr;
    NexusLinkData_t*  s_Nexus    = nullptr;

    // Mumble is shared memory the game rewrites every frame: copy the bits we read.
    bool TextboxFocused() { return s_Mumble && s_Mumble->Context.IsTextboxFocused; }
    bool GameFocused()    { return s_Mumble && s_Mumble->Context.IsGameFocused; }
    bool InGameplay()     { return s_Nexus && s_Nexus->IsGameplay; }
    bool IsCommander()    { return s_Identity && s_Identity->IsCommander; }

    bool AnyModifierHeld()
    {
        const int keys[] = {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN};
        for (int k : keys)
            if (GetAsyncKeyState(k) & 0x8000) return true;
        return false;
    }

    template <class Pred>
    bool WaitUntil(Pred aPred, int aTimeoutMs, const std::atomic<bool>& aStop)
    {
        const auto until = Clock::now() + std::chrono::milliseconds(aTimeoutMs);
        while (!aPred())
        {
            if (aStop || Clock::now() >= until) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        }
        return true;
    }

    // --- Running code on the game's window thread ----------------------------------------------
    // The worker posts a private message to the game window; our WndProc hook (which runs on the
    // window thread) picks it up, runs the queued call, and consumes the message. Keys and text
    // then reach the game exactly as from its own message loop, with nothing touching the OS
    // input state.

    std::atomic<HWND> s_Hwnd{nullptr};
    UINT              s_CallMsg = 0;

    std::mutex              s_CallMutex;
    std::condition_variable s_CallCv;
    std::function<void()>   s_Call;
    uint64_t                s_CallSeq  = 0; // id of the call waiting in s_Call
    uint64_t                s_CallDone = 0; // id of the last finished call

    UINT OnWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        (void)wParam;
        (void)lParam;
        if (!s_Hwnd.load()) s_Hwnd = hWnd;
        if (uMsg != s_CallMsg || s_CallMsg == 0) return uMsg; // everything else passes through untouched

        std::function<void()> call;
        uint64_t seq;
        {
            std::lock_guard<std::mutex> lock(s_CallMutex);
            call = std::move(s_Call);
            s_Call = nullptr;
            seq = s_CallSeq;
        }
        if (call) call();
        {
            std::lock_guard<std::mutex> lock(s_CallMutex);
            s_CallDone = seq;
        }
        s_CallCv.notify_all();
        return 0; // ours: don't pass it to the game
    }

    // Runs aCall on the window thread and waits for it. aCall must own its data (capture by value):
    // after a timeout the worker moves on while the call may still run later.
    bool OnWindowThread(std::function<void()> aCall)
    {
        const HWND hwnd = s_Hwnd.load();
        if (!hwnd || !s_CallMsg) return false;
        uint64_t seq;
        {
            std::lock_guard<std::mutex> lock(s_CallMutex);
            s_Call = std::move(aCall);
            seq = ++s_CallSeq;
        }
        if (!PostMessageW(hwnd, s_CallMsg, 0, 0))
        {
            std::lock_guard<std::mutex> lock(s_CallMutex);
            s_Call = nullptr;
            return false;
        }
        std::unique_lock<std::mutex> lock(s_CallMutex);
        const bool done = s_CallCv.wait_for(lock, std::chrono::milliseconds(kWindowCallTimeoutMs),
                                            [seq] { return s_CallDone >= seq; });
        if (!done && s_CallSeq == seq) s_Call = nullptr; // too late: don't run it
        return done;
    }

    // Window-thread helpers (only called inside OnWindowThread).
    void PostToGame(HWND h, UINT msg, WPARAM w, LPARAM l) { g_Api->WndProc_SendToGameOnly(h, msg, w, l); }

    void TypeUtf8(HWND h, const std::string& aText)
    {
        const int n = MultiByteToWideChar(CP_UTF8, 0, aText.data(), static_cast<int>(aText.size()), nullptr, 0);
        if (n <= 0) return;
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, aText.data(), static_cast<int>(aText.size()), w.data(), n);
        for (wchar_t c : w) PostToGame(h, WM_CHAR, static_cast<WPARAM>(c), 1);
    }

    // A full key press as the game's message loop would see it: down, char, up.
    void PressKey(HWND h, UINT aVk, wchar_t aChar)
    {
        const LPARAM scan = static_cast<LPARAM>(MapVirtualKeyW(aVk, MAPVK_VK_TO_VSC)) << 16;
        PostToGame(h, WM_KEYDOWN, aVk, 1 | scan);
        if (aChar) PostToGame(h, WM_CHAR, static_cast<WPARAM>(aChar), 1 | scan);
        PostToGame(h, WM_KEYUP, aVk, 1 | scan | (LPARAM(1) << 30) | (LPARAM(1) << 31));
    }

    bool Type(const std::string& aText)
    {
        const HWND h = s_Hwnd.load();
        return OnWindowThread([h, aText] { TypeUtf8(h, aText); });
    }

    bool Key(UINT aVk, wchar_t aChar)
    {
        const HWND h = s_Hwnd.load();
        return OnWindowThread([h, aVk, aChar] { PressKey(h, aVk, aChar); });
    }

    // --- The worker ----------------------------------------------------------------------------

    std::thread             s_Thread;
    std::atomic<bool>       s_Stop{false};
    std::atomic<bool>       s_Busy{false};
    std::mutex              s_JobMutex;
    std::condition_variable s_JobCv;
    std::optional<std::vector<MacroTree::Line>> s_Job; // the macro's lines, placeholders not filled in yet
    uint64_t                s_JobMacro = 0;

    std::mutex      s_StatusMutex;
    Sender::Status  s_Status;

    void SetStatus(Sender::EState aState, size_t aLine, size_t aLines, const std::string& aMsg, uint64_t aMacro)
    {
        std::lock_guard<std::mutex> lock(s_StatusMutex);
        s_Status.State   = aState;
        s_Status.Line    = aLine;
        s_Status.Lines   = aLines;
        s_Status.Message = aMsg;
        s_Status.MacroId = aMacro;
        if (aState == Sender::EState::Done || aState == Sender::EState::Failed) s_Status.Seq++;
    }

    // Why the game isn't ready for us to type, or empty if it is.
    std::string NotReadyReason()
    {
        if (!s_Mumble || !s_Nexus)   return "Game data isn't available yet.";
        if (!InGameplay())           return "Not in game (loading screen or character select).";
        if (!GameFocused())          return "The game window isn't focused.";
        if (TextboxFocused())        return "A text box is open in game. Close it (or finish typing) and try again.";
        if (!s_Hwnd.load())          return "The game window isn't known yet. Try again in a moment.";
        return {};
    }

    // Closes chat if *we* left it open, so the game is back to normal. Never touches a chat box
    // the user opened themselves (e.g. a send refused because they're typing). At most one Escape
    // per macro: Mumble lags a frame behind, and a second Escape on the open game opens its menu.
    bool s_EscapeSent    = false;
    bool s_WeOpenedChat  = false; // set when we press the chat key, cleared once chat has closed

    void CloseChatIfOpen()
    {
        if (!s_WeOpenedChat || s_EscapeSent || !TextboxFocused()) return;
        s_EscapeSent = true;
        Key(VK_ESCAPE, 0x1B);
    }

    std::string Run(const SendPlan::Plan& aPlan, uint64_t aMacro)
    {
        size_t sent = 0;
        size_t currentLine = static_cast<size_t>(-1);
        size_t lineNumber = 0;

        for (const SendPlan::Step& step : aPlan.Steps)
        {
            if (s_Stop) return "Stopped.";
            if (step.Line != currentLine)
            {
                currentLine = step.Line;
                lineNumber++;
                SetStatus(Sender::EState::Sending, lineNumber, aPlan.LinesToSend, {}, aMacro);
            }
            const std::string where = "Line " + std::to_string(step.Line + 1) + ": ";

            switch (step.Type)
            {
            case EStep::OpenChat:
            case EStep::OpenSquadBroadcast:
            {
                // Never act while the user holds a modifier: Shift+Enter, Ctrl+... would change what our keys do.
                if (!WaitUntil([] { return !AnyModifierHeld(); }, kModifierWaitMs, s_Stop))
                    return where + "a Shift, Ctrl, Alt or Windows key is held down. Release it and try again.";
                const std::string why = NotReadyReason();
                if (!why.empty()) return where + why;

                const EGameBinds bind = step.Type == EStep::OpenChat ? GB_UiChatFocus : GB_UiSquadBroadcastChatFocus;
                if (!g_Api->GameBinds_IsBound(bind))
                    return step.Type == EStep::OpenChat
                        ? "Nexus doesn't know your \"Chat\" key. Set it in Nexus > Options > Game Keybinds."
                        : "Nexus doesn't know your \"Squad Broadcast\" chat key. Set it in Nexus > Options > Game Keybinds.";
                g_Api->GameBinds_Press(bind);
                std::this_thread::sleep_for(std::chrono::milliseconds(kBindHoldMs));
                g_Api->GameBinds_Release(bind);
                s_WeOpenedChat = true;
                break;
            }
            case EStep::WaitChatOpen:
                if (!WaitUntil(TextboxFocused, kChatOpenTimeoutMs, s_Stop))
                    return where + "chat didn't open. Check your chat key in Nexus > Options > Game Keybinds.";
                break;
            case EStep::Type:
                if (!TextboxFocused()) { return where + "chat closed while typing."; }
                if (!Type(step.Text)) { CloseChatIfOpen(); return where + "couldn't reach the game window."; }
                break;
            case EStep::KeyTab:
                // Only ever inside a focused chat box, never on the open game (where Tab targets enemies).
                if (!TextboxFocused()) return where + "chat closed before the whisper message.";
                if (!Key(VK_TAB, L'\t')) { CloseChatIfOpen(); return where + "couldn't reach the game window."; }
                break;
            case EStep::KeyEnter:
                if (!TextboxFocused()) return where + "chat closed before sending.";
                if (!Key(VK_RETURN, L'\r')) { CloseChatIfOpen(); return where + "couldn't reach the game window."; }
                break;
            case EStep::WaitChatClosed:
                if (!WaitUntil([] { return !TextboxFocused(); }, kChatCloseTimeoutMs, s_Stop))
                {
                    CloseChatIfOpen();
                    return where + "the game didn't take the message; chat was closed.";
                }
                s_WeOpenedChat = false; // the game closed it: it's no longer ours to close
                sent++;
                break;
            case EStep::Pause:
                std::this_thread::sleep_for(std::chrono::milliseconds(step.Ms));
                break;
            }
        }
        (void)sent;
        return {};
    }

    // --- Placeholders: the outside world for Placeholders::Expand -------------------------------

    constexpr uintmax_t kMaxTextFileBytes = 1024 * 1024; // {txt} files: plenty for chat lines

    std::string Utf8(const wchar_t* aText, int aLen)
    {
        const int n = WideCharToMultiByte(CP_UTF8, 0, aText, aLen, nullptr, 0, nullptr, nullptr);
        if (n <= 0) return {};
        std::string s(static_cast<size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, aText, aLen, s.data(), n, nullptr, nullptr);
        return s;
    }

    bool ReadAddonTextFile(const std::string& aRelPath, std::vector<std::string>& aLines, std::string& aError)
    {
        const std::filesystem::path dir = Storage::AddonDir();
        if (dir.empty()) { aError = "the addon folder isn't available."; return false; }
        const std::filesystem::path file = dir / std::filesystem::u8path(aRelPath);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(file, ec))
        {
            aError = "\"" + aRelPath + "\" wasn't found in <GW2>\\addons\\GW2_Chat_Messages\\.";
            return false;
        }
        if (std::filesystem::file_size(file, ec) > kMaxTextFileBytes || ec)
        {
            aError = "\"" + aRelPath + "\" is too big (over 1 MB).";
            return false;
        }
        std::ifstream in(file, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!in && !in.eof()) { aError = "\"" + aRelPath + "\" couldn't be read."; return false; }
        if (data.size() >= 3 && data.compare(0, 3, "\xEF\xBB\xBF") == 0) data.erase(0, 3); // UTF-8 BOM
        size_t start = 0;
        while (start <= data.size())
        {
            size_t end = data.find('\n', start);
            if (end == std::string::npos) end = data.size();
            std::string line = data.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            aLines.push_back(std::move(line));
            start = end + 1;
        }
        return true;
    }

    Placeholders::Context PlaceholderContext()
    {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        Placeholders::Context c;
        c.Year = st.wYear;
        c.Month = st.wMonth;
        c.Day = st.wDay;
        c.Hour = st.wHour;
        c.Minute = st.wMinute;
        wchar_t day[64] = {};
        const int n = GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, L"dddd", day, 64, nullptr);
        c.Weekday = n > 1 ? Utf8(day, n - 1) : std::string("Today");
        c.AddonVersion = std::string(kAddonName) + " " + kVersionText;
        c.Random = [](long long aMin, long long aMax) {
            static std::mt19937_64 rng{std::random_device{}()};
            return std::uniform_int_distribution<long long>(aMin, aMax)(rng);
        };
        c.ReadLines = ReadAddonTextFile;
        c.MapName = [](std::string& aName, std::string& aError) {
            const uint32_t id = s_Mumble ? s_Mumble->Context.MapID : 0;
            return MapNames::Get(id, aName, aError);
        };
        c.FetchUrl = [](const std::string& aUrl, std::string& aBody, std::string& aError) {
            const Http::Response r = Http::Get(aUrl);
            if (!r.Ok())
            {
                aError = r.Error.empty() ? "the download failed." : r.Error;
                return false;
            }
            aBody = r.Body;
            return true;
        };
        return c;
    }

    // Fills in placeholders, then builds the steps. Runs on the worker: {map} and {json} may wait
    // on the network. Returns the error, or "" with aPlan ready.
    std::string Prepare(std::vector<MacroTree::Line> aLines, SendPlan::Plan& aPlan)
    {
        const Placeholders::Context ctx = PlaceholderContext();
        for (size_t i = 0; i < aLines.size(); i++)
        {
            for (std::string* field : {&aLines[i].Message, &aLines[i].WhisperTo})
            {
                if (s_Stop) return "Stopped.";
                Placeholders::Result r = Placeholders::Expand(*field, ctx);
                if (!r.Error.empty()) return "Line " + std::to_string(i + 1) + ": " + r.Error;
                *field = std::move(r.Text);
            }
        }
        // Built from the filled-in text, so the 199-character check is on what's really typed.
        aPlan = SendPlan::Build(aLines, IsCommander());
        return aPlan.Error;
    }

    void WorkerMain()
    {
        for (;;)
        {
            std::vector<MacroTree::Line> lines;
            uint64_t macro = 0;
            {
                std::unique_lock<std::mutex> lock(s_JobMutex);
                s_JobCv.wait(lock, [] { return s_Stop.load() || s_Job.has_value(); });
                if (s_Stop) return;
                lines = std::move(*s_Job);
                s_Job.reset();
                macro = s_JobMacro;
            }

            s_EscapeSent   = false;
            s_WeOpenedChat = false;
            SendPlan::Plan plan;
            std::string error = Prepare(std::move(lines), plan);
            if (error.empty()) error = Run(plan, macro);
            if (error.empty())
            {
                SetStatus(Sender::EState::Done, plan.LinesToSend, plan.LinesToSend,
                          plan.LinesToSend == 1 ? "Sent 1 line." : "Sent " + std::to_string(plan.LinesToSend) + " lines.", macro);
            }
            else
            {
                CloseChatIfOpen();
                SetStatus(Sender::EState::Failed, 0, plan.LinesToSend, error, macro);
                LogF(LOGL_WARNING, "Send failed: %s", error.c_str());
            }
            s_Busy = false;
        }
    }
}

void Sender::Init()
{
    s_Mumble   = static_cast<Mumble::Data*>(g_Api->DataLink_Get(DL_MUMBLE_LINK));
    s_Identity = static_cast<Mumble::Identity*>(g_Api->DataLink_Get(DL_MUMBLE_LINK_IDENTITY));
    s_Nexus    = static_cast<NexusLinkData_t*>(g_Api->DataLink_Get(DL_NEXUS_LINK));
    s_CallMsg  = RegisterWindowMessageW(L"GW2ChatMessages.Sender.Call"); // unique; can't clash with the game's own
    s_Stop     = false;
    s_Busy     = false;
    SetStatus(EState::Idle, 0, 0, {}, 0);
    s_Thread   = std::thread(WorkerMain);
    g_Api->WndProc_Register(OnWndProc);
}

void Sender::Shutdown()
{
    s_Stop = true;
    s_JobCv.notify_all();
    // Bounded: every wait in the worker times out (game steps ~1 s, a {map}/{json} download ~3 s per stage).
    if (s_Thread.joinable()) s_Thread.join();
    g_Api->WndProc_Deregister(OnWndProc);
    {
        std::lock_guard<std::mutex> lock(s_CallMutex);
        s_Call = nullptr;
    }
    s_Hwnd = nullptr;
    s_Mumble = nullptr;
    s_Identity = nullptr;
    s_Nexus = nullptr;
}

bool Sender::Start(uint64_t aMacroId, const std::vector<MacroTree::Line>& aLines, std::string& aError)
{
    if (s_Busy.exchange(true))
    {
        aError = "Already sending a macro.";
        return false;
    }
    // Placeholders and the plan are prepared on the worker (they may need the network); every
    // step there checks the game again, so this is only the quick "can we start at all" test.
    const std::string why = NotReadyReason();
    if (!why.empty())
    {
        s_Busy = false;
        aError = why;
        SetStatus(EState::Failed, 0, 0, aError, aMacroId);
        return false;
    }
    SetStatus(EState::Sending, 0, 0, {}, aMacroId); // Line 0: still preparing
    {
        std::lock_guard<std::mutex> lock(s_JobMutex);
        s_Job = aLines;
        s_JobMacro = aMacroId;
    }
    s_JobCv.notify_one();
    return true;
}

Sender::Status Sender::GetStatus()
{
    std::lock_guard<std::mutex> lock(s_StatusMutex);
    return s_Status;
}

bool Sender::IsBusy() { return s_Busy.load(); }
