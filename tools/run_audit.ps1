# The clang part of the security audit (docs/security-audit.md): clang-tidy (checks in
# .clang-tidy) and clangd --check on our own sources, using build\compile_commands.json.
# Reports go to build\audit\ (ignored by git); a summary is printed at the end.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_audit.ps1            # all of src\
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_audit.ps1 -Changed   # files changed vs main
#   ... -Base <ref>   compare with another branch, tag or commit instead of main
#   ... -Tests        also audit tests\*.cpp
# Exit code: 0 = no findings, 1 = findings to review, 2 = the audit couldn't run.
param([switch]$Changed, [string]$Base = "main", [switch]$Tests)

$ErrorActionPreference = "Stop"
$root   = Split-Path -Parent $PSScriptRoot
$llvm   = "C:\Program Files\LLVM\bin"
$tidy   = Join-Path $llvm "clang-tidy.exe"
$clangd = Join-Path $llvm "clangd.exe"
foreach ($exe in $tidy, $clangd) {
    if (-not (Test-Path $exe)) { Write-Host "Not found: $exe (install LLVM)"; exit 2 }
}

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root "tools\compile_commands.ps1")
$db = Join-Path $root "build\compile_commands.json"
if (-not (Test-Path $db)) { Write-Host "No build\compile_commands.json"; exit 2 }
if (-not (Test-Path (Join-Path $root "build\obj\Release\gen\BuildStampGen.h"))) {
    Write-Host "Build Release once first (the build stamp header is missing)."; exit 2
}

# Which files.
$dirs = @("src") + $(if ($Tests) { @("tests") } else { @() })
if ($Changed) {
    $names = & git -C $root diff --name-only "$Base...HEAD" -- $dirs
    $names += & git -C $root diff --name-only -- $dirs   # uncommitted edits too
    $files = $names | Where-Object { $_ -match '\.cpp$' } | Sort-Object -Unique |
             ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }
    # A changed header: audit the .cpp next to it.
    $headers = $names | Where-Object { $_ -match '\.h$' } | ForEach-Object { Join-Path $root ($_ -replace '\.h$', '.cpp') } | Where-Object { Test-Path $_ }
    $files = @($files) + @($headers) | Sort-Object -Unique
} else {
    $files = $dirs | ForEach-Object { Get-ChildItem (Join-Path $root $_) -Filter *.cpp } | ForEach-Object { $_.FullName }
}
if (-not $files) { Write-Host "No C++ files to audit."; exit 0 }

$out = Join-Path $root "build\audit"
New-Item -ItemType Directory -Force $out | Out-Null
$tidyLog   = Join-Path $out "clang-tidy.txt"
$clangdLog = Join-Path $out "clangd.txt"
Set-Content $tidyLog "" ; Set-Content $clangdLog ""

$tidyFindings = @()
$clangdErrors = @()
$bdir = Join-Path $root "build"
# The tools write their progress to stderr; that's not a failure, so don't stop on it.
$ErrorActionPreference = "Continue"
$i = 0
foreach ($f in $files) {
    $i++
    $rel = $f.Substring($root.Length + 1)
    Write-Host "[$i/$($files.Count)] $rel"

    $t = & $tidy -p $bdir --quiet $f 2>&1 | Out-String
    Add-Content $tidyLog "===== $rel`r`n$t"
    $tidyFindings += $t -split "`n" | Where-Object { $_ -match '(warning|error): .*\[([\w.-]+)\]' }

    $c = & $clangd "--check=$f" "--compile-commands-dir=$bdir" 2>&1 | Out-String
    Add-Content $clangdLog "===== $rel`r`n$c"
    # "tweak:" lines are clangd testing its own refactorings, not problems in the code.
    $clangdErrors += $c -split "`n" | Where-Object { $_ -match '^E\[' -and $_ -notmatch 'tweak:' } | ForEach-Object { "$rel : $_" }
}

# The same finding is printed once per file that includes the header: count it once.
$tidyFindings = $tidyFindings | ForEach-Object { $_.Trim() } | Sort-Object -Unique
Write-Host ""
Write-Host "clang-tidy: $($tidyFindings.Count) findings in $($files.Count) files (full output: build\audit\clang-tidy.txt)"
$tidyFindings | ForEach-Object { if ($_ -match '\[([\w.-]+)\]\s*$') { $Matches[1] } } | Group-Object | Sort-Object Count -Descending |
    ForEach-Object { Write-Host ("  {0,4}  {1}" -f $_.Count, $_.Name) }
Write-Host "clangd: $($clangdErrors.Count) errors (full output: build\audit\clangd.txt)"
$clangdErrors | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" }
Set-Content (Join-Path $out "findings.txt") ($tidyFindings + $clangdErrors)
Write-Host "All findings, one per line: build\audit\findings.txt"

if ($tidyFindings.Count -or $clangdErrors.Count) { exit 1 } else { exit 0 }
