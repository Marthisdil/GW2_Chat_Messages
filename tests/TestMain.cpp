// Runs every standalone test. Build and run: tools\run_tests.ps1 (CI runs it too).

#include "TestCheck.h"

void RunMacroTreeTests();
void RunChatLineTests();
void RunSendPlanTests();
void RunHotkeyTests();
void RunFilterTests();
void RunPlaceholderTests();
void RunExchangeTests();
void RunLinkSyncTests();
void RunTextSizeTests();
void RunErrorReportTests();
void RunTrashTests();
void RunSearchTests();
void RunLocalizationTests();
void RunPathTextTests();
void RunGameKeysTests();
void RunMapCatalogTests();
void RunCooldownTests();
void RunExamplesTests();
void RunBackupsTests();
void RunQuickListsTests();
void RunAdvancedTests();
void RunFormatTests();

int main()
{
    RunMacroTreeTests();
    RunChatLineTests();
    RunSendPlanTests();
    RunHotkeyTests();
    RunFilterTests();
    RunPlaceholderTests();
    RunExchangeTests();
    RunLinkSyncTests();
    RunTextSizeTests();
    RunErrorReportTests();
    RunTrashTests();
    RunSearchTests();
    RunLocalizationTests();
    RunPathTextTests();
    RunGameKeysTests();
    RunMapCatalogTests();
    RunCooldownTests();
    RunExamplesTests();
    RunBackupsTests();
    RunQuickListsTests();
    RunAdvancedTests();
    RunFormatTests();
    std::printf("%d checks, %d failures\n", g_TestChecks, g_TestFailures);
    return g_TestFailures == 0 ? 0 : 1;
}
