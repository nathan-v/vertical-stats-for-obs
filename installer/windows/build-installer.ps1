<#
.SYNOPSIS
    Stage the built plugin and produce the Windows installer (and zip).

.DESCRIPTION
    1. cmake --install  ->  release\vertical-stats-for-obs\{bin,data}
    2. ISCC.exe         ->  release\vertical-stats-for-obs-<ver>-windows-x64-installer.exe

    Run from anywhere; paths are resolved relative to this script.
    Requires Inno Setup 6 (winget install JRSoftware.InnoSetup) and a completed
    `cmake --build --preset windows-x64 --config RelWithDebInfo`.

.PARAMETER Configuration
    CMake configuration that was built. Default RelWithDebInfo.
#>
param(
    [string]$Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$BuildDir = Join-Path $ProjectRoot 'build_x64'
$ReleaseDir = Join-Path $ProjectRoot 'release'

$BuildSpec = Get-Content (Join-Path $ProjectRoot 'buildspec.json') -Raw | ConvertFrom-Json
$Version = $BuildSpec.version
$Name = $BuildSpec.name

# --- locate tools --------------------------------------------------------
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    # Not on PATH; look in the usual places (Kitware installer, Visual Studio's
    # bundled copy, and a pip-installed cmake under any Python version).
    $candidates = @(
        "$env:ProgramFiles\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:APPDATA\Python\Python3*\Scripts\cmake.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python3*\Scripts\cmake.exe"
    )
    foreach ($pattern in $candidates) {
        $hit = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($hit) { $cmake = Get-Command $hit.FullName; break }
    }
}
if (-not $cmake) { throw 'cmake not found on PATH or in the usual install locations' }

$iscc = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if (-not $iscc) {
    foreach ($p in @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
                     "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe")) {
        if (Test-Path $p) { $iscc = Get-Command $p; break }
    }
}
if (-not $iscc) { throw 'ISCC.exe (Inno Setup 6) not found. Install with: winget install JRSoftware.InnoSetup' }

# --- stage ---------------------------------------------------------------
if (-not (Test-Path $BuildDir)) { throw "Build directory $BuildDir not found. Build the plugin first." }
Write-Host "Staging $Name $Version ($Configuration) into $ReleaseDir"
& $cmake.Source --install $BuildDir --config $Configuration --prefix $ReleaseDir
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed ($LASTEXITCODE)" }

# --- installer -----------------------------------------------------------
$iss = Join-Path $PSScriptRoot "$Name.iss"
Write-Host "Compiling installer with $($iscc.Source)"
& $iscc.Source "/DVersion=$Version" "/DSourceDir=$ReleaseDir\$Name" "/DOutputDir=$ReleaseDir" "/Q" $iss
if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)" }

$exe = Join-Path $ReleaseDir "$Name-$Version-windows-x64-installer.exe"
Write-Host "Installer: $exe ($([math]::Round((Get-Item $exe).Length / 1KB)) KB)"
