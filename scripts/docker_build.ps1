<#
.SYNOPSIS
    Ignite Engine build entrypoint — runs inside the Windows Docker container.

.DESCRIPTION
    This script is invoked by `docker run` in GitHub Actions CI.
    It expects:
      - The repo workspace mounted at C:\workspace
      - VULKAN_SDK, FBX_SDK_PATH, PATH all pre-configured by the image
      - premake5.exe and msbuild.exe on PATH

    Usage (in CI):
        docker run --rm -v "$PWD:C:\workspace" evangelionz/ignite-engine:latest \
            powershell -File C:\workspace\scripts\docker_build.ps1
#>
[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$Platform      = "x64"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# ── Banner ─────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  Ignite Engine – Docker Container Build"              -ForegroundColor Cyan
Write-Host "  Configuration : $Configuration"                      -ForegroundColor Cyan
Write-Host "  Platform      : $Platform"                           -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""

# ── Working directory ──────────────────────────────────────────────────────
$workspace = "C:\workspace"
if (-not (Test-Path $workspace)) {
    throw "Workspace not found at $workspace. Did you mount the repo with -v?"
}
Set-Location $workspace

# ── Verify environment ─────────────────────────────────────────────────────
Write-Host "[ENV] VULKAN_SDK    = $env:VULKAN_SDK"
Write-Host "[ENV] FBX_SDK_PATH  = $env:FBX_SDK_PATH"

$tools = @{
    "premake5.exe" = "Premake5"
    "msbuild.exe"  = "MSBuild"
}
foreach ($bin in $tools.Keys) {
    $found = Get-Command $bin -ErrorAction SilentlyContinue
    if (-not $found) { throw "$($tools[$bin]) ($bin) not found on PATH. Check the Docker image." }
    Write-Host "[ENV] $($tools[$bin]) = $($found.Source)"
}

if (-not (Test-Path $env:VULKAN_SDK)) {
    throw "VULKAN_SDK path does not exist: $env:VULKAN_SDK"
}
if (-not (Test-Path $env:FBX_SDK_PATH)) {
    throw "FBX_SDK_PATH path does not exist: $env:FBX_SDK_PATH"
}

Write-Host ""

# ── Step 1: Run Premake ────────────────────────────────────────────────────
Write-Host "[1/2] Generating project files with Premake..." -ForegroundColor Yellow
& premake5.exe --file=scripts/premake5.lua vs2022
if ($LASTEXITCODE -ne 0) {
    throw "Premake failed with exit code $LASTEXITCODE"
}
Write-Host "[1/2] Premake OK." -ForegroundColor Green
Write-Host ""

# ── Step 2: Build with MSBuild ─────────────────────────────────────────────
Write-Host "[2/2] Building solution with MSBuild..." -ForegroundColor Yellow

# Locate the solution file (.slnx preferred, fallback to .sln)
$sln = Get-ChildItem $workspace -Filter "*.slnx" | Select-Object -First 1
if (-not $sln) {
    $sln = Get-ChildItem $workspace -Filter "*.sln" | Select-Object -First 1
}
if (-not $sln) {
    throw "No .slnx or .sln file found in $workspace"
}
Write-Host "[2/2] Using solution: $($sln.Name)"

& msbuild.exe $sln.FullName `
    /p:Configuration=$Configuration `
    /p:Platform=$Platform `
    /m `
    /nologo `
    /consoleloggerparameters:Summary
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}
Write-Host "[2/2] Build SUCCEEDED." -ForegroundColor Green
Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  Build complete."                                      -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
