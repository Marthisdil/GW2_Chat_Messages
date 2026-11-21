# Writes build\compile_commands.json for clangd (code navigation and errors in editors).
# MSBuild doesn't write one, so this reads the .vcxproj (its .cpp files, include folders and
# defines) and tools\run_tests.ps1 (the test files). clangd finds the file in build\ by itself.
# The file is never committed (build\ is ignored).
#
# Run by the .vcxproj before every build; it only rewrites the file when it's missing or older
# than the .vcxproj, run_tests.ps1 or this script. -Force rewrites it anyway.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\compile_commands.ps1 [-Force]
# It never fails the build: problems are printed as a warning and it exits 0.
param([switch]$Force)

$ErrorActionPreference = "Stop"
try {
    $root  = Split-Path -Parent $PSScriptRoot
    $proj  = Join-Path $root "GW2_Chat_Messages.vcxproj"
    $tests = Join-Path $root "tools\run_tests.ps1"
    $out   = Join-Path $root "build\compile_commands.json"

    if (-not $Force -and (Test-Path $out)) {
        $made = (Get-Item $out).LastWriteTimeUtc
        $newer = @($proj, $tests, $PSCommandPath) | Where-Object { (Get-Item $_).LastWriteTimeUtc -gt $made }
        if (-not $newer) { exit 0 }
    }

    [xml]$x = Get-Content -Raw $proj
    $ns = @{ m = "http://schemas.microsoft.com/developer/msbuild/2003" }

    # The Release settings: the unconditional ItemDefinitionGroup plus the Release one.
    $intDir = "$root\build\obj\Release\"
    function Expand([string]$s) {
        $s.Replace('$(ProjectDir)', "$root\").Replace('$(SolutionDir)', "$root\").Replace('$(IntDir)', $intDir)
    }
    function Values([string]$xpath) {
        $vals = @()
        foreach ($n in (Select-Xml -Xml $x -Namespace $ns -XPath $xpath)) {
            $cond = $n.Node.ParentNode.ParentNode.GetAttribute("Condition")
            if ($cond -and $cond -notmatch "Release") { continue }
            $vals += ($n.Node.InnerText -split ";") | Where-Object { $_ -and $_ -notmatch '^%\(' }
        }
        $vals
    }
    $includes = Values "//m:ItemDefinitionGroup/m:ClCompile/m:AdditionalIncludeDirectories" | ForEach-Object { (Expand $_).TrimEnd("\") }
    $defines  = Values "//m:ItemDefinitionGroup/m:ClCompile/m:PreprocessorDefinitions"

    # clang-cl understands MSVC flags, so the commands look like the real build.
    $common = @("clang-cl", "/std:c++17", "/EHsc", "/MT", "/W3", "--target=x86_64-pc-windows-msvc")
    $projArgs = $common + ($defines | ForEach-Object { "/D$_" }) + ($includes | ForEach-Object { "/I$_" })

    $entries = [ordered]@{}
    foreach ($n in (Select-Xml -Xml $x -Namespace $ns -XPath "//m:ItemGroup/m:ClCompile/@Include")) {
        $file = Join-Path $root $n.Node.Value
        $entries[$file] = $projArgs + @("/c", $file)
    }

    # The unit tests: the same flags as tools\run_tests.ps1 (src, tests and json includes, /utf-8).
    $testArgs = $common + @("/utf-8", "/I$root\src", "/I$root\tests", "/I$root\vendor\json\single_include")
    foreach ($m in [regex]::Matches((Get-Content -Raw $tests), '"(tests\\[^"]+\.cpp)"')) {
        $file = Join-Path $root $m.Groups[1].Value
        if (-not $entries.Contains($file)) { $entries[$file] = $testArgs + @("/c", $file) }
    }

    $list = foreach ($k in $entries.Keys) { [ordered]@{ directory = $root; file = $k; arguments = $entries[$k] } }
    New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
    $json = ConvertTo-Json -InputObject @($list) -Depth 5
    [System.IO.File]::WriteAllText($out, $json, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "compile_commands.json: $($entries.Count) files"
} catch {
    Write-Host "warning: compile_commands.json not written: $($_.Exception.Message)"
}
exit 0
