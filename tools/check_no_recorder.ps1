# Checks that a DLL doesn't contain the frame recorder (src/PerfRecorder.*), which is only for
# builds made on the owner's PC. CI runs it on every DLL it builds, and the Release workflow
# before publishing, so no published release can have it.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_no_recorder.ps1 -Dll build\Release\GW2_Chat_Messages.dll
# Exits 1 if the recorder's marker text is in the file. -ExpectRecorder turns the check around
# (for a local build, to see that the check would find it).
param(
    [Parameter(Mandatory = $true)][string]$Dll,
    [switch]$ExpectRecorder
)
$ErrorActionPreference = "Stop"

# The same text as kMarker in src/PerfRecorder.cpp.
$marker = "CHATMSG-FRAME-RECORDER"
if (-not (Test-Path -LiteralPath $Dll)) { throw "No such file: $Dll" }
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Dll).Path)
# Latin-1 maps every byte to one character, so the marker's offset is the byte offset.
$text  = [System.Text.Encoding]::GetEncoding(28591).GetString($bytes)
$found = $text.Contains($marker)

if ($ExpectRecorder) {
    if ($found) { Write-Host "The frame recorder is in $Dll (as expected for a local build)."; exit 0 }
    Write-Host "The frame recorder is NOT in $Dll."; exit 1
}
if ($found) {
    Write-Host "::error::$Dll contains the frame recorder ($marker). It must never be in a CI or released build."
    exit 1
}
Write-Host "No frame recorder in $Dll."
exit 0
