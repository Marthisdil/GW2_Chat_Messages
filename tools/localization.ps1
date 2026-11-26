# Keeps localization\ in step with the code (docs/design.md, "Languages").
#
# The addon's English text is the key: every T("..."), TR("...") and TX("...") literal in src\ is a text to
# translate (the "##id" part of an ImGui label is left out). This script lists them and
#   -Check (default): fails if localization\en.json or template.json is out of date with the code,
#                     and reports what each language file is missing or has extra;
#   -Write:           rewrites en.json and template.json, and brings every other language file in
#                     line: new texts are added with "" (English is shown until translated), and a
#                     translated text the code no longer uses (its English was reworded or removed)
#                     moves to "Unused", so the translator can reuse it; the addon ignores "Unused".
#                     A text that comes back gets its translation back from "Unused".
#                     Every file gets "Format" (Localization::kFormat; docs/design.md, "Languages").
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\localization.ps1 [-Write]
param([switch]$Write)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dir  = Join-Path $root "localization"
$utf8 = New-Object System.Text.UTF8Encoding($false)
Add-Type -AssemblyName System.Web.Extensions # a case-sensitive JSON reader ("Folder" and "folder" are both texts)

function Unescape([string]$lit, [string]$where) {
    $sb = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $lit.Length; $i++) {
        $c = $lit[$i]
        if ($c -ne '\') { [void]$sb.Append($c); continue }
        $i++
        switch -CaseSensitive ($lit[$i]) {
            'n'  { [void]$sb.Append("`n") }
            't'  { [void]$sb.Append("`t") }
            '"'  { [void]$sb.Append('"') }
            "'"  { [void]$sb.Append("'") }
            '\'  { [void]$sb.Append('\') }
            default { throw "$where : unsupported escape \$($lit[$i]) in a translated text" }
        }
    }
    return $sb.ToString()
}

# Every text, in order of first use: entry.cpp, Ui.cpp, then the other files by name.
$files = @(Get-ChildItem (Join-Path $root "src") -Include *.cpp, *.h -Recurse | Sort-Object Name)
$first = @("entry.cpp", "Ui.cpp")
$files = @($files | Where-Object { $first -contains $_.Name } | Sort-Object { [array]::IndexOf($first, $_.Name) }) +
         @($files | Where-Object { $first -notcontains $_.Name })
$lit   = '"(?:[^"\\\r\n]|\\.)*"'
$call  = [regex]('\bT[XR]?\(\s*((?:' + $lit + '\s*)+)\)')
$piece = [regex]$lit
$texts = New-Object System.Collections.Generic.List[string]
$seen  = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
# Drops // comments (outside strings), so a T("...") in a comment isn't a text.
function StripComments([string]$code) {
    $sb = New-Object System.Text.StringBuilder
    $inStr = $false; $inChar = $false
    for ($i = 0; $i -lt $code.Length; $i++) {
        $c = $code[$i]
        if ($inStr -or $inChar) {
            [void]$sb.Append($c)
            if ($c -eq '\' -and $i + 1 -lt $code.Length) { $i++; [void]$sb.Append($code[$i]); continue }
            if ($inStr -and $c -eq '"') { $inStr = $false }
            elseif ($inChar -and $c -eq "'") { $inChar = $false }
            continue
        }
        if ($c -eq '/' -and $i + 1 -lt $code.Length -and $code[$i + 1] -eq '/') {
            while ($i -lt $code.Length -and $code[$i] -ne "`n") { $i++ }
            [void]$sb.Append("`n")
            continue
        }
        if ($c -eq '"') { $inStr = $true } elseif ($c -eq "'") { $inChar = $true }
        [void]$sb.Append($c)
    }
    return $sb.ToString()
}

foreach ($f in $files) {
    $code = StripComments ([IO.File]::ReadAllText($f.FullName, $utf8))
    foreach ($m in $call.Matches($code)) {
        $text = ""
        foreach ($p in $piece.Matches($m.Groups[1].Value)) { $text += Unescape $p.Value.Substring(1, $p.Value.Length - 2) $f.Name }
        $cut = $text.IndexOf("##")
        if ($cut -ge 0) { $text = $text.Substring(0, $cut) }
        $plain = [regex]::Replace($text, '%[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z)?[sdiuxXfgc%]', '')
        if ($plain -notmatch '[A-Za-z]') { continue } # "%s", "##id": nothing to translate
        if ($seen.Add($text)) { $texts.Add($text) }
    }
}

$version = "?"
$verFile = Join-Path $root "src\Version.h"
if (Test-Path $verFile) {
    $vm = [regex]::Match([IO.File]::ReadAllText($verFile), 'CHATMSG_VER_TEXT\s+"([^"]+)"')
    if ($vm.Success) { $version = $vm.Groups[1].Value }
}

function JsonString([string]$s) {
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append('"')
    foreach ($c in $s.ToCharArray()) {
        switch ($c) {
            '"'     { [void]$sb.Append('\"') }
            '\'     { [void]$sb.Append('\\') }
            "`n"    { [void]$sb.Append('\n') }
            "`t"    { [void]$sb.Append('\t') }
            default { if ([int]$c -lt 0x20) { [void]$sb.Append(('\u{0:x4}' -f [int]$c)) } else { [void]$sb.Append($c) } }
        }
    }
    [void]$sb.Append('"')
    return $sb.ToString()
}

$format = 1
$locHeader = Join-Path $root "src\Localization.h"
$fm = [regex]::Match([IO.File]::ReadAllText($locHeader), 'constexpr\s+int\s+kFormat\s*=\s*(\d+)')
if ($fm.Success) { $format = [int]$fm.Groups[1].Value } else { throw "kFormat not found in src\Localization.h" }

# A language file, keys in $texts order. $values: key -> text ("" = not translated). $unused: old
# English -> translation, written after Texts (sorted, so the file stays stable).
function LanguageJson([string]$id, [string]$name, [string]$ver, [string[]]$about, $values, $unused) {
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("{")
    $lines.Add('  "Format": ' + $format + ',')
    $lines.Add('  "Identifier": ' + (JsonString $id) + ',')
    $lines.Add('  "DisplayName": ' + (JsonString $name) + ',')
    if ($ver) { $lines.Add('  "AddonVersion": ' + (JsonString $ver) + ',') }
    if ($about) {
        $lines.Add('  "About": [')
        for ($i = 0; $i -lt $about.Count; $i++) { $lines.Add('    ' + (JsonString $about[$i]) + $(if ($i -lt $about.Count - 1) { ',' } else { '' })) }
        $lines.Add('  ],')
    }
    $lines.Add('  "Texts": {')
    for ($i = 0; $i -lt $texts.Count; $i++) {
        $k = $texts[$i]
        $v = if ($values.ContainsKey($k)) { [string]$values[$k] } else { "" }
        $lines.Add('    ' + (JsonString $k) + ': ' + (JsonString $v) + $(if ($i -lt $texts.Count - 1) { ',' } else { '' }))
    }
    $old = @()
    if ($unused) { $old = @($unused.Keys | Sort-Object -CaseSensitive) }
    if ($old.Count -gt 0) {
        $lines.Add('  },')
        $lines.Add('  "Unused": {')
        for ($i = 0; $i -lt $old.Count; $i++) {
            $lines.Add('    ' + (JsonString $old[$i]) + ': ' + (JsonString ([string]$unused[$old[$i]])) + $(if ($i -lt $old.Count - 1) { ',' } else { '' }))
        }
    }
    $lines.Add('  }')
    $lines.Add('}')
    return ($lines -join "`n") + "`n"
}

function ReadLanguage([string]$path) {
    $ser = New-Object System.Web.Script.Serialization.JavaScriptSerializer
    $ser.MaxJsonLength = 4MB
    return $ser.DeserializeObject([IO.File]::ReadAllText($path, $utf8))
}

$english = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([StringComparer]::Ordinal)
foreach ($t in $texts) { $english[$t] = $t }
$empty = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([StringComparer]::Ordinal)
$templateAbout = @(
    "Template for a new language for Chat Messages. See CONTRIBUTING.md in the addon's GitHub repository.",
    "1. Save a copy as <code>.json, e.g. nl.json. Set Identifier to the language code (2-3 lower-case letters, optionally -region: nl, pt-br) and DisplayName to the language's own name (Nederlands).",
    "2. Under Texts, fill in the right-hand side of each line. The left-hand side is the English text: never change it. Leave a text empty to show it in English.",
    "3. Keep every %-code (%s, %d, %zu, %.1f...) exactly as it is and in the same order, keep \\n line breaks, and keep names like Nexus, GW2, macros.json and Ctrl+F as they are. A text whose %-codes don't match is not used.",
    "4. To try it, put the file in <GW2>\\addons\\GW2_Chat_Messages\\localization\\ and click Reload language files in the addon's Nexus options."
)

$outputs = [ordered]@{
    "en.json"       = (LanguageJson "en" "English (US)" "" @("Every English text the addon shows. Written by tools\localization.ps1 from the code: don't edit by hand.") $english $null)
    "template.json" = (LanguageJson "xx" "Your language's name" "" $templateAbout $empty $null)
}

$problems = 0
foreach ($name in $outputs.Keys) {
    $path = Join-Path $dir $name
    $now  = if (Test-Path $path) { [IO.File]::ReadAllText($path, $utf8) -replace "`r`n", "`n" } else { "" }
    if ($now -ne $outputs[$name]) {
        if ($Write) { [IO.File]::WriteAllText($path, $outputs[$name], $utf8); Write-Host "Wrote localization\$name" }
        else { Write-Host "localization\$name is out of date with the code: run tools\localization.ps1 -Write"; $problems++ }
    }
}

# The other language files: what's missing, what's extra; -Write brings them in line.
foreach ($f in Get-ChildItem $dir -Filter *.json | Where-Object { $_.Name -ne "en.json" -and $_.Name -ne "template.json" } | Sort-Object Name) {
    $doc = ReadLanguage $f.FullName
    $tx  = $doc["Texts"]
    $missing = 0; $extra = 0
    foreach ($t in $texts) { if (-not $tx.ContainsKey($t) -or [string]::IsNullOrEmpty([string]$tx[$t])) { $missing++ } }
    foreach ($k in $tx.Keys) { if (-not $english.ContainsKey($k)) { $extra++ } }
    $kept = if ($doc.ContainsKey("Unused") -and $doc["Unused"] -is [System.Collections.IDictionary]) { $doc["Unused"].Count } else { 0 }
    Write-Host ("{0}: {1} of {2} texts translated, {3} not translated, {4} no longer used, {5} kept under Unused" -f $f.Name, ($texts.Count - $missing), $texts.Count, $missing, $extra, $kept)
    if ($Write) {
        $values = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([StringComparer]::Ordinal)
        $unused = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([StringComparer]::Ordinal)
        if ($doc.ContainsKey("Unused") -and $doc["Unused"] -is [System.Collections.IDictionary]) {
            foreach ($k in $doc["Unused"].Keys) { $unused[$k] = [string]$doc["Unused"][$k] }
        }
        foreach ($k in $tx.Keys) {
            $v = [string]$tx[$k]
            if ($english.ContainsKey($k)) { $values[$k] = $v }
            elseif ($v -ne "") { $unused[$k] = $v } # reworded or removed: kept for the translator
        }
        foreach ($k in @($unused.Keys)) {
            if ($english.ContainsKey($k)) { # the text is back: so is its translation
                if (-not $values.ContainsKey($k) -or $values[$k] -eq "") { $values[$k] = $unused[$k] }
                [void]$unused.Remove($k)
            }
        }
        $about = if ($doc.ContainsKey("About")) { [string[]]$doc["About"] } else { $null }
        $ver = if ($doc.ContainsKey("AddonVersion")) { [string]$doc["AddonVersion"] } else { $version }
        $out = LanguageJson ([string]$doc["Identifier"]) ([string]$doc["DisplayName"]) $ver $about $values $unused
        $now = [IO.File]::ReadAllText($f.FullName, $utf8) -replace "`r`n", "`n"
        if ($now -ne $out) { [IO.File]::WriteAllText($f.FullName, $out, $utf8); Write-Host "  updated $($f.Name)" }
    }
}
Write-Host "$($texts.Count) texts in the code."
if ($problems) { exit 1 }
exit 0
