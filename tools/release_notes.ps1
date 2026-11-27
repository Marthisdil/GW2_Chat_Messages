# Writes one version's notes from CHANGELOG.md, for the GitHub release's text.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\release_notes.ps1 -Version 2026.11.21.1412 -Out notes.md
# The section is the one tools\set_version.ps1 wrote: "## [2026.11.21.1412] - 2026-11-21 (Dev)",
# up to the next "## [" heading. Fails if it's missing or empty (set_version.ps1 wasn't run, or
# CHANGELOG.md wasn't committed with Version.h).
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Changelog
)
$ErrorActionPreference = "Stop"
# $PSScriptRoot is empty in a param default on Windows PowerShell 5.1, so default it here.
if (-not $Changelog) { $Changelog = Join-Path (Split-Path -Parent $PSScriptRoot) "CHANGELOG.md" }

$lines = [System.IO.File]::ReadAllLines($Changelog)
$head = "## [$Version]"
$start = -1
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i].StartsWith($head)) { $start = $i; break }
}
if ($start -lt 0) { throw "CHANGELOG.md has no '$head' section. Run tools\set_version.ps1 and commit CHANGELOG.md with src\Version.h." }

$body = @()
for ($i = $start + 1; $i -lt $lines.Length; $i++) {
    if ($lines[$i].StartsWith("## [")) { break }
    $body += $lines[$i]
}
$text = ($body -join "`n").Trim()
if (-not $text) { throw "The '$head' section of CHANGELOG.md is empty." }

$full = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Out)
[System.IO.File]::WriteAllText($full, $text + "`n", (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Notes for $Version`: $($body.Count) lines from $($lines[$start])"
