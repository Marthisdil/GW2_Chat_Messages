# Puts captured screenshots into the docs (see docs/screenshots.md).
# For every row marked "Needed" whose PNG now exists in docs\images\:
#   - each "<!-- screenshot: name.png ... -->" placeholder in README.md and docs\wiki\*.md is
#     replaced with the image (README: docs/images/..., wiki pages: ../images/...),
#   - the row is marked "Captured (date)".
# Also lists PNGs in docs\images\ that aren't in the table. Changes nothing else.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\update_screenshots.ps1
$ErrorActionPreference = "Stop"
$root   = Split-Path -Parent $PSScriptRoot
$list   = Join-Path $root "docs\screenshots.md"
$images = Join-Path $root "docs\images"
$utf8   = New-Object System.Text.UTF8Encoding $false

function Read-Text($path)  { [System.IO.File]::ReadAllText($path, $utf8) }
function Write-Text($path, $text) { [System.IO.File]::WriteAllText($path, $text, $utf8) }

$table = Read-Text $list
$rowPattern = '(?m)^\| (\d+) \| `([^`]+\.png)` \|(.*)\| Needed \|\r?$'
$needed = [regex]::Matches($table, $rowPattern) | ForEach-Object { $_.Groups[2].Value }
$known  = [regex]::Matches($table, '(?m)^\| \d+ \| `([^`]+\.png)` \|') | ForEach-Object { $_.Groups[1].Value }

$docs = @(Join-Path $root "README.md") + (Get-ChildItem (Join-Path $root "docs\wiki") -Filter *.md | ForEach-Object { $_.FullName })
$today = Get-Date -Format "yyyy-MM-dd"
$done = @()

foreach ($name in $needed) {
    if (-not (Test-Path (Join-Path $images $name))) { continue }
    $escaped = [regex]::Escape($name)
    $block = "<!-- screenshot: $escaped \(docs/screenshots\.md\) -->\r?\n> \S+ \*Screenshot to come: (.+?)\.\*"
    foreach ($doc in $docs) {
        $text = Read-Text $doc
        if ($text -notmatch $block) { continue }
        $prefix = if ($doc -like "*\docs\wiki\*") { "../images" } else { "docs/images" }
        $text = [regex]::Replace($text, $block, {
            param($m)
            $alt = $m.Groups[1].Value
            $alt = $alt.Substring(0, 1).ToUpper() + $alt.Substring(1)
            "![$alt]($prefix/$name)"
        })
        Write-Text $doc $text
        Write-Output "  $name -> $(Split-Path -Leaf $doc)"
    }
    $row = '(?m)^(\| \d+ \| `' + $escaped + '` \|.*\| )Needed( \|)\r?$'
    $table = [regex]::Replace($table, $row, "`${1}Captured ($today)`${2}")
    $done += $name
}
if ($done.Count -gt 0) { Write-Text $list $table }

$extra = @(Get-ChildItem $images -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $known -notcontains $_.Name } | ForEach-Object { $_.Name })
$missing = @($needed | Where-Object { $done -notcontains $_ })
Write-Output "Captured now: $($done.Count). Still needed: $($missing.Count)."
if ($extra.Count -gt 0) { Write-Output "Not in the list (check the name): $($extra -join ', ')" }
