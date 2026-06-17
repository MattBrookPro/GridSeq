<#
    build-plugin-msvc.ps1 - build the GridSeq plugin locally on Windows.

    JUCE on Windows is supported on MSVC (not MinGW - JUCE 8's Direct2D renderer
    needs Windows SDK headers MinGW doesn't ship). This script finds the MSVC
    build tools, imports their environment, and builds the VST3 / Standalone.

    Usage:
        powershell -ExecutionPolicy Bypass -File tools\build-plugin-msvc.ps1
        powershell -ExecutionPolicy Bypass -File tools\build-plugin-msvc.ps1 -Validate
#>
param([switch]$Validate)

$ErrorActionPreference = "Stop"
$root      = Split-Path -Parent $PSScriptRoot
$buildDir  = Join-Path $root "build-msvc"

# --- Locate the newest MSVC vcvars64.bat -------------------------------------
$roots = @("${env:ProgramFiles}\Microsoft Visual Studio",
           "${env:ProgramFiles(x86)}\Microsoft Visual Studio") | Where-Object { Test-Path $_ }
$vcvars = Get-ChildItem -Path $roots -Recurse -Filter vcvars64.bat -ErrorAction SilentlyContinue |
          Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $vcvars) { throw "Could not find vcvars64.bat - install Visual Studio Build Tools with the C++ workload." }
Write-Host "Using MSVC environment: $vcvars"

# Import the MSVC environment into this session.
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
}
$env:CC = "cl"; $env:CXX = "cl"

# --- Prefer the VS-bundled CMake/Ninja, else fall back to PATH ---------------
$vsInstall = Split-Path (Split-Path (Split-Path (Split-Path $vcvars)))
$cmake = Join-Path $vsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja = Join-Path $vsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if (-not (Test-Path $cmake)) { $cmake = "cmake" }

$gen = @("-G", "Ninja")
if (Test-Path $ninja) { $gen += "-DCMAKE_MAKE_PROGRAM=$ninja" }

# --- Configure + build -------------------------------------------------------
& $cmake -S $root -B $buildDir @gen -DCMAKE_BUILD_TYPE=Release `
    -DGRIDSEQ_BUILD_PLUGIN=ON -DGRIDSEQ_BUILD_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw "configure failed" }

& $cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$vst3 = Join-Path $buildDir "plugin\GridSeqPlugin_artefacts\Release\VST3\GridSeq.vst3"
Write-Host "`nBuilt: $vst3"

# --- Optional: validate with pluginval --------------------------------------
if ($Validate) {
    $pvDir = Join-Path $buildDir "_pluginval"
    $pv    = Join-Path $pvDir "pluginval.exe"
    if (-not (Test-Path $pv)) {
        New-Item -ItemType Directory -Force -Path $pvDir | Out-Null
        $zip = Join-Path $pvDir "pluginval.zip"
        Invoke-WebRequest -Uri "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" -OutFile $zip
        Expand-Archive -Path $zip -DestinationPath $pvDir -Force
    }
    Write-Host "`nValidating with pluginval (strictness 5)..."
    # Gate on the reported result, not the exit code (pluginval can return
    # non-zero from shutdown on some hosts even when all tests pass).
    $out = & $pv --strictness-level 5 --validate-in-process --validate $vst3 2>&1 | Out-String
    Write-Host $out
    if ($out -match 'SUCCESS' -and $out -notmatch 'FAILED') { Write-Host "pluginval: PASS" }
    else { throw "pluginval reported failures" }
}
