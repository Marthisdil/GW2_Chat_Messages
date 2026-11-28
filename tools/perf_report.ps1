# Sums up a frame recorder file (<GW2>\addons\GW2_Chat_Messages\perf\perf_..._UTC.csv): how long the
# game's frames took, how much of that was this addon, which parts of it, and the slowest frames
# with what happened around them. The recorder is only in builds made on the owner's PC
# (src/PerfRecorder.h, docs/performance.md).
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\perf_report.ps1 -File <path to perf_...csv>
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\perf_report.ps1 -Dir <GW2>\addons\GW2_Chat_Messages\perf
#     (-Dir: the file in that folder written last)
# -Top N: how many of the slowest frames to list (default 10).
# Works in Windows PowerShell 5.1 and PowerShell 7.
param(
    [string]$File,
    [string]$Dir,
    [int]$Top = 10
)
$ErrorActionPreference = "Stop"

if (-not $File) {
    if (-not $Dir) { throw "Give -File <perf_...csv> or -Dir <the perf folder>." }
    $newest = Get-ChildItem -LiteralPath $Dir -Filter "perf_*_UTC.csv" | Sort-Object LastWriteTimeUtc, Name -Descending | Select-Object -First 1
    if (-not $newest) { throw "No perf_*_UTC.csv in $Dir" }
    $File = $newest.FullName
}
if (-not (Test-Path -LiteralPath $File)) { throw "No such file: $File" }

$lines  = Get-Content -LiteralPath $File -Encoding UTF8
$header = $lines | Where-Object { $_.StartsWith("#") } | Select-Object -First 1
$rows   = $lines | Where-Object { -not $_.StartsWith("#") -and $_.Trim() -ne "" } | ConvertFrom-Csv
$inv    = [System.Globalization.CultureInfo]::InvariantCulture
function Num($v) { if ($null -eq $v -or $v -eq "") { return 0.0 } return [double]::Parse($v, $inv) }
function F($v, $d = 2) { return $v.ToString("F$d", $inv) }

$seconds = @($rows | Where-Object { $_.kind -eq "second" })
$spikes  = @($rows | Where-Object { $_.kind -eq "spike" })
$events  = @($rows | Where-Object { $_.kind -eq "event" })
if ($seconds.Count -eq 0) { throw "The file has no 'second' rows yet (it records one per second)." }

$sections = @("languages_ms", "poll_ms", "library_ms", "quickmenu_ms", "windows_ms", "saves_ms", "options_ms")

# Frame-weighted averages over the 'second' rows.
$frames = 0.0; $game = 0.0; $addon = 0.0; $slow = 0.0
$gameMax = 0.0; $addonMax = 0.0; $wndMax = 0.0; $addonP99Max = 0.0
$sec = @{}; foreach ($s in $sections) { $sec[$s] = 0.0 }
$ctx = @{}
foreach ($r in $seconds) {
    $n = Num $r.frames
    $frames += $n
    $game   += $n * (Num $r.game_avg_ms)
    $addon  += $n * (Num $r.addon_avg_ms)
    $slow   += Num $r.addon_slow
    $gameMax     = [math]::Max($gameMax, (Num $r.game_max_ms))
    $addonMax    = [math]::Max($addonMax, (Num $r.addon_max_ms))
    $addonP99Max = [math]::Max($addonP99Max, (Num $r.addon_p99_ms))
    $wndMax      = [math]::Max($wndMax, (Num $r.wndproc_max_us))
    foreach ($s in $sections) { $sec[$s] += $n * (Num $r.$s) }
    $parts = @()
    $parts += $(if ($r.gameplay -eq "1") { "in gameplay" } else { "not in gameplay" })
    if ($r.library_open -eq "1") { $parts += "library open" }
    if ($r.menu_open -eq "1") { $parts += "quick menu open" }
    if ($r.sending -eq "1") { $parts += "sending" }
    $key = $parts -join ", "
    if (-not $ctx.ContainsKey($key)) { $ctx[$key] = @{ Frames = 0.0; Game = 0.0; Addon = 0.0; Seconds = 0 } }
    $ctx[$key].Frames += $n
    $ctx[$key].Game   += $n * (Num $r.game_avg_ms)
    $ctx[$key].Addon  += $n * (Num $r.addon_avg_ms)
    $ctx[$key].Seconds++
}
if ($frames -le 0) { throw "No frames recorded." }
$gameAvg  = $game / $frames
$addonAvg = $addon / $frames
$duration = Num ($seconds[-1].t_sec)

Write-Host ""
Write-Host "Frame recording: $(Split-Path -Leaf $File)"
if ($header) { Write-Host "  $($header.TrimStart('#').Trim())" }
Write-Host ("  {0} s recorded, {1} frames, {2} spike rows, {3} events" -f (F $duration 0), $frames, $spikes.Count, $events.Count)
Write-Host ""
Write-Host "Game"
Write-Host ("  {0} ms a frame on average ({1} fps); slowest frame {2} ms" -f (F $gameAvg), (F (1000.0 / $gameAvg) 0), (F $gameMax 1))
Write-Host ""
Write-Host "This addon"
Write-Host ("  {0} ms a frame on average ({1}% of the frame); slowest {2} ms; worst second's 99th percentile {3} ms" -f (F $addonAvg 3), (F (100.0 * $addonAvg / $gameAvg) 2), (F $addonMax 2), (F $addonP99Max 2))
Write-Host ("  frames over 2 ms: {0} ({1}%)" -f $slow, (F (100.0 * $slow / $frames) 3))
Write-Host ("  slowest pass through the hotkey hook: {0} us" -f (F $wndMax 1))
Write-Host ""
Write-Host "Where the addon's time went (average ms a frame)"
foreach ($s in ($sections | Sort-Object { -$sec[$_] })) {
    Write-Host ("  {0,-14} {1}" -f ($s -replace "_ms$", ""), (F ($sec[$s] / $frames) 3))
}
Write-Host ""
Write-Host "By what you were doing"
foreach ($k in ($ctx.Keys | Sort-Object { -$ctx[$_].Frames })) {
    $c = $ctx[$k]
    Write-Host ("  {0,-45} {1,5} s   game {2} ms   addon {3} ms" -f $k, $c.Seconds, (F ($c.Game / $c.Frames)), (F ($c.Addon / $c.Frames) 3))
}

if ($spikes.Count -gt 0) {
    Write-Host ""
    Write-Host "Slowest frames for the addon (top $Top), with events within a second"
    foreach ($sp in ($spikes | Sort-Object { -(Num $_.addon_max_ms) } | Select-Object -First $Top)) {
        $t = Num $sp.t_sec
        $biggest = $sections | Sort-Object { -(Num $sp.$_) } | Select-Object -First 1
        $near = @($events | Where-Object { [math]::Abs((Num $_.t_sec) - $t) -le 1.0 } | ForEach-Object { $_.note }) -join "; "
        Write-Host ("  t={0,8} s ({1})  addon {2} ms (most in {3}: {4} ms)  game {5} ms  {6}" -f (F $t 1), $sp.utc, (F (Num $sp.addon_max_ms)), ($biggest -replace "_ms$", ""), (F (Num $sp.$biggest)), (F (Num $sp.game_max_ms) 1), $near)
    }
    $gameSpikes = @($spikes | Where-Object { (Num $_.game_max_ms) -gt 50 })
    if ($gameSpikes.Count -gt 0) {
        $ours = @($gameSpikes | Where-Object { (Num $_.addon_max_ms) -gt 2 })
        Write-Host ("  game frames over 50 ms: {0}; the addon took over 2 ms in {1} of them" -f $gameSpikes.Count, $ours.Count)
    }
}

$marks = @($events | Where-Object { $_.note -like "mark *" })
if ($marks.Count -gt 0) {
    Write-Host ""
    Write-Host "Marks"
    foreach ($m in $marks) {
        $t = Num $m.t_sec
        $around = @($seconds | Where-Object { [math]::Abs((Num $_.t_sec) - $t) -le 2.0 })
        $a = if ($around.Count) { ($around | ForEach-Object { Num $_.addon_max_ms } | Measure-Object -Maximum).Maximum } else { 0 }
        Write-Host ("  {0} at t={1} s ({2}): slowest addon frame within 2 s {3} ms" -f $m.note, (F $t 1), $m.utc, (F $a))
    }
}
Write-Host ""
