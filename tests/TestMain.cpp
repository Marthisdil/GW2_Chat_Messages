// Runs every standalone test. Build and run: tools\run_tests.ps1 (CI runs it too).

#include "TestCheck.h"

void RunMacroTreeTests();
void RunChatLineTests();
void RunSendPlanTests();
void RunHotkeyTests();
void RunFilterTests();

int main()
{
    RunMacroTreeTests();
    RunChatLineTests();
    RunSendPlanTests();
    RunHotkeyTests();
    RunFilterTests();
    std::printf("%d checks, %d failures\n", g_TestChecks, g_TestFailures);
    return g_TestFailures == 0 ? 0 : 1;
}
