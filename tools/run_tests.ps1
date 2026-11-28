# Builds and runs the standalone unit tests (tests\*.cpp + the Nexus-free sources they test).
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_tests.ps1
# Output goes to build\tests\ (ignored by git). Exits non-zero if a test fails.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio C++ tools not found" }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

$out = Join-Path $root "build\tests"
New-Item -ItemType Directory -Force -Path $out | Out-Null

# Every test file, plus the Nexus-free sources under test. Add new ones here.
$sources = @(
    "tests\TestMain.cpp", "tests\MacroTreeTests.cpp", "tests\ChatLineTests.cpp", "tests\SendPlanTests.cpp", "tests\HotkeyTests.cpp", "tests\FilterTests.cpp", "tests\PlaceholderTests.cpp", "tests\ExchangeTests.cpp", "tests\LinkSyncTests.cpp", "tests\TextSizeTests.cpp", "tests\ErrorReportTests.cpp", "tests\TrashTests.cpp", "tests\SearchTests.cpp", "tests\LocalizationTests.cpp", "tests\PathTextTests.cpp", "tests\GameKeysTests.cpp", "tests\MapCatalogTests.cpp", "tests\CooldownTests.cpp", "tests\ExamplesTests.cpp", "tests\BackupsTests.cpp", "tests\QuickListsTests.cpp", "tests\AdvancedTests.cpp", "tests\FormatTests.cpp", "tests\EditorLayoutTests.cpp", "tests\WelcomeTests.cpp", "tests\WorkQueueTests.cpp", "tests\PerfTests.cpp",
    "src\MacroTree.cpp", "src\ChatLine.cpp", "src\SendPlan.cpp", "src\Hotkey.cpp", "src\Placeholders.cpp", "src\Exchange.cpp", "src\LinkSync.cpp", "src\TextSize.cpp", "src\ErrorReport.cpp", "src\Search.cpp", "src\Localization.cpp", "src\PathText.cpp", "src\GameKeys.cpp", "src\MapCatalog.cpp", "src\Cooldown.cpp", "src\Examples.cpp", "src\Backups.cpp", "src\QuickLists.cpp", "src\Advanced.cpp", "src\EditorLayout.cpp", "src\Welcome.cpp", "src\ReleaseNotes.cpp", "src\WorkQueue.cpp", "src\Perf.cpp"
) | ForEach-Object { "`"$(Join-Path $root $_)`"" }
$includes = "/I `"$root\src`" /I `"$root\tests`" /I `"$root\vendor\json\single_include`""
# /Z7 keeps debug info in the .obj files (compile-time PDBs fail inside Dropbox folders).
# /utf-8: test sources contain UTF-8 string literals.
$cl = "cl /nologo /std:c++17 /utf-8 /EHsc /W3 /Z7 /MTd $includes $($sources -join ' ') /Fo`"$out\\`" /Fe`"$out\UnitTests.exe`""

cmd /c "`"$vcvars`" >nul && $cl"
if ($LASTEXITCODE -ne 0) { throw "Test build failed" }

# From the repository root: the localization tests read localization\*.json.
Push-Location $root
try { & "$out\UnitTests.exe"; $code = $LASTEXITCODE } finally { Pop-Location }
if ($code -ne 0) { exit $code }

# localization\en.json and template.json must list exactly the texts in the code.
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "localization.ps1")
exit $LASTEXITCODE
