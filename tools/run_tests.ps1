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

$sources = @("tests\MacroTreeTests.cpp", "src\MacroTree.cpp") | ForEach-Object { "`"$(Join-Path $root $_)`"" }
$includes = "/I `"$root\src`" /I `"$root\vendor\json\single_include`""
# /Z7 keeps debug info in the .obj files (compile-time PDBs fail inside Dropbox folders).
$cl = "cl /nologo /std:c++17 /EHsc /W3 /Z7 /MTd $includes $($sources -join ' ') /Fo`"$out\\`" /Fe`"$out\MacroTreeTests.exe`""

cmd /c "`"$vcvars`" >nul && $cl"
if ($LASTEXITCODE -ne 0) { throw "Test build failed" }

& "$out\MacroTreeTests.exe"
exit $LASTEXITCODE
