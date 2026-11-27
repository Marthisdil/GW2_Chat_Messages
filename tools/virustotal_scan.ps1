<#
.SYNOPSIS
  Uploads files to VirusTotal, waits for the scan, and writes a summary (Markdown and JSON).

.DESCRIPTION
  Self-contained, so it can be copied into other projects unchanged (docs/virustotal.md).
  Runs on Windows PowerShell 5.1 and PowerShell 7 (Windows, Linux, macOS).

  For each file it:
    1. computes the SHA-256;
    2. asks VirusTotal whether it already has a finished scan of that exact file (a rerun of a
       release, say) and reuses it;
    3. otherwise uploads the file (up to 32 MB) and polls the analysis until it's finished or the
       time is up.

  Uses the free public API: 4 requests a minute and 500 a day, so it waits between calls and
  backs off on HTTP 429. An uploaded file becomes visible to VirusTotal's community: only scan
  files you are about to publish anyway.

  The API key comes from -ApiKey or the VT_API_KEY environment variable. It is sent only as the
  x-apikey header to www.virustotal.com, and never printed or written to the outputs.

.PARAMETER Path
  One or more files to scan.

.PARAMETER MarkdownOut
  Writes a Markdown section (a table with each file's SHA-256, the result and the link to the
  VirusTotal report, plus the engines that flagged anything) for release notes.

.PARAMETER JsonOut
  Writes the results as JSON, for a workflow to decide on (see the "Results" note below).

.PARAMETER TimeoutMinutes
  How long to wait for each file's scan. Default 20.

.PARAMETER PollSeconds
  Time between status checks (at least 16, to stay under 4 requests a minute). Default 20.

.NOTES
  Results: each file gives Name, Sha256, Link, Status ("completed" or "timeout"), Malicious,
  Suspicious, Undetected, Harmless, Engines (the engines that gave a verdict) and Flagged (a
  list of "Engine: verdict" for each malicious or suspicious result).

  Exit codes: 0 every scan finished (whatever it found: the caller decides what's too many),
  2 at least one scan didn't finish in time, 1 an error (no key, a file missing, the API refused).

.EXAMPLE
  $env:VT_API_KEY = '...'   # your own key; never commit it
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\virustotal_scan.ps1 -Path build\Release\GW2_Chat_Messages.dll -MarkdownOut vt.md -JsonOut vt.json
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$Path,
    [string]$ApiKey = $env:VT_API_KEY,
    [string]$MarkdownOut,
    [string]$JsonOut,
    [int]$TimeoutMinutes = 20,
    [int]$PollSeconds = 20
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2

$Base = "https://www.virustotal.com/api/v3"
$GuiBase = "https://www.virustotal.com/gui/file"
$MaxUploadBytes = 32MB
if ($PollSeconds -lt 16) { $PollSeconds = 16 }

if ([string]::IsNullOrWhiteSpace($ApiKey)) {
    Write-Error "No VirusTotal API key: set VT_API_KEY (a GitHub Actions secret in CI) or pass -ApiKey."
    exit 1
}

# Windows PowerShell 5.1 defaults to old TLS versions.
try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 } catch { }
Add-Type -AssemblyName System.Net.Http

$client = New-Object System.Net.Http.HttpClient
$client.Timeout = [TimeSpan]::FromMinutes(5)
$client.DefaultRequestHeaders.Add("x-apikey", $ApiKey.Trim())
$client.DefaultRequestHeaders.Add("accept", "application/json")
$ApiKey = $null

# One API call. Returns @{ Code = <int>; Json = <object or $null> }. Retries on 429 (rate
# limit) and on 5xx, waiting a minute each time, up to 5 times.
function Invoke-Vt {
    param([string]$Method, [string]$Url, [string]$UploadFile)
    for ($try = 1; $try -le 6; $try++) {
        $content = $null
        $stream = $null
        try {
            if ($Method -eq "POST") {
                $content = New-Object System.Net.Http.MultipartFormDataContent
                $stream = [System.IO.File]::OpenRead($UploadFile)
                $part = New-Object System.Net.Http.StreamContent($stream)
                $part.Headers.ContentType = [System.Net.Http.Headers.MediaTypeHeaderValue]::Parse("application/octet-stream")
                $content.Add($part, "file", [System.IO.Path]::GetFileName($UploadFile))
                $resp = $client.PostAsync($Url, $content).GetAwaiter().GetResult()
            } else {
                $resp = $client.GetAsync($Url).GetAwaiter().GetResult()
            }
            $code = [int]$resp.StatusCode
            $body = $resp.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        } finally {
            if ($stream) { $stream.Dispose() }
            if ($content) { $content.Dispose() }
        }
        if (($code -eq 429 -or $code -ge 500) -and $try -lt 6) {
            Write-Host "  VirusTotal answered $code; waiting 60 s (try $try of 5)."
            Start-Sleep -Seconds 60
            continue
        }
        $json = $null
        if ($body) { try { $json = $body | ConvertFrom-Json } catch { } }
        return @{ Code = $code; Json = $json }
    }
}

function Get-ErrorText($r) {
    if ($r.Json -and ($r.Json.PSObject.Properties.Name -contains "error")) {
        return "$($r.Json.error.code): $($r.Json.error.message)"
    }
    return "HTTP $($r.Code)"
}

# Per-engine verdicts ({ "EngineName": { category, result, ... }, ... }) -> "Engine: verdict" list.
function Get-Flagged($results) {
    $out = @()
    if (-not $results) { return $out }
    foreach ($p in $results.PSObject.Properties) {
        $cat = $p.Value.category
        if ($cat -eq "malicious" -or $cat -eq "suspicious") {
            $verdict = $p.Value.result
            if (-not $verdict) { $verdict = $cat }
            $out += "$($p.Name): $verdict ($cat)"
        }
    }
    return ($out | Sort-Object)
}

function New-Result($file, $sha, $status, $stats, $results) {
    $g = { param($n) if ($stats -and ($stats.PSObject.Properties.Name -contains $n)) { [int]$stats.$n } else { 0 } }
    $mal = & $g "malicious"; $sus = & $g "suspicious"; $und = & $g "undetected"; $harm = & $g "harmless"
    [pscustomobject]@{
        Name       = [System.IO.Path]::GetFileName($file)
        Sha256     = $sha
        Link       = "$GuiBase/$sha"
        Status     = $status
        Malicious  = $mal
        Suspicious = $sus
        Undetected = $und
        Harmless   = $harm
        Engines    = $mal + $sus + $und + $harm
        Flagged    = @(Get-Flagged $results)
    }
}

# UTF-8 without a byte order mark (Windows PowerShell 5.1's -Encoding UTF8 adds one).
function Write-Utf8([string]$File, [string]$Text) {
    $full = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($File)
    [System.IO.File]::WriteAllText($full, $Text, (New-Object System.Text.UTF8Encoding($false)))
}

$all = @()
$exit = 0
foreach ($f in $Path) {
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { Write-Error "File not found: $f"; exit 1 }
    $full = (Resolve-Path -LiteralPath $f).Path
    $size = (Get-Item -LiteralPath $full).Length
    $sha = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "$([System.IO.Path]::GetFileName($full)): SHA-256 $sha"

    # 1. Already scanned? (404 = VirusTotal has never seen this file.)
    $r = Invoke-Vt GET "$Base/files/$sha"
    if ($r.Code -eq 200 -and $r.Json.data.attributes.PSObject.Properties.Name -contains "last_analysis_date") {
        Write-Host "  Already on VirusTotal; using its last scan."
        $a = $r.Json.data.attributes
        $all += New-Result $full $sha "completed" $a.last_analysis_stats $a.last_analysis_results
        continue
    }
    if ($r.Code -ne 200 -and $r.Code -ne 404) { Write-Error "Looking up $sha failed: $(Get-ErrorText $r)"; exit 1 }

    # 2. Upload.
    if ($size -gt $MaxUploadBytes) { Write-Error "$full is over 32 MB (the simple upload's limit)."; exit 1 }
    Write-Host "  Uploading ($size bytes)..."
    $r = Invoke-Vt POST "$Base/files" $full
    if ($r.Code -ne 200) { Write-Error "Upload failed: $(Get-ErrorText $r)"; exit 1 }
    $analysisId = $r.Json.data.id

    # 3. Wait for the analysis.
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    $done = $null
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds $PollSeconds
        $r = Invoke-Vt GET "$Base/analyses/$analysisId"
        if ($r.Code -ne 200) { Write-Error "Checking the analysis failed: $(Get-ErrorText $r)"; exit 1 }
        $status = $r.Json.data.attributes.status
        Write-Host "  Status: $status"
        if ($status -eq "completed") { $done = $r.Json.data.attributes; break }
    }
    if ($done) {
        $all += New-Result $full $sha "completed" $done.stats $done.results
    } else {
        Write-Host "  Not finished after $TimeoutMinutes minutes; the link will show the result when it is."
        $all += New-Result $full $sha "timeout" $null $null
        $exit = 2
    }
}
$client.Dispose()

foreach ($x in $all) {
    if ($x.Status -eq "completed") {
        Write-Host "$($x.Name): $($x.Malicious) malicious, $($x.Suspicious) suspicious, of $($x.Engines) engines. $($x.Link)"
        foreach ($fl in $x.Flagged) { Write-Host "  $fl" }
    } else {
        Write-Host "$($x.Name): scan still running. $($x.Link)"
    }
}

if ($JsonOut) {
    Write-Utf8 $JsonOut (ConvertTo-Json -InputObject @($all) -Depth 4)
}
if ($MarkdownOut) {
    $md = @("### VirusTotal scan", "",
            "Each file in this release was uploaded to [VirusTotal](https://www.virustotal.com), which checks it with many antivirus engines. Click a result for the full report.", "",
            "| File | SHA-256 | Result |", "|---|---|---|")
    foreach ($x in $all) {
        if ($x.Status -eq "completed") {
            $n = $x.Malicious + $x.Suspicious
            $res = "[$n of $($x.Engines) engines flagged it]($($x.Link))"
        } else {
            $res = "[Scan still running when this was published]($($x.Link))"
        }
        $md += "| ``$($x.Name)`` | ``$($x.Sha256)`` | $res |"
    }
    $flaggedAny = @($all | Where-Object { $_.Flagged.Count -gt 0 })
    if ($flaggedAny.Count -gt 0) {
        $md += ""
        $md += "Engines that flagged a file (a few false positives are common for addons that hook a game's window):"
        $md += ""
        foreach ($x in $flaggedAny) { foreach ($fl in $x.Flagged) { $md += "- ``$($x.Name)``: $fl" } }
    }
    Write-Utf8 $MarkdownOut (($md -join "`n") + "`n")
}
exit $exit
