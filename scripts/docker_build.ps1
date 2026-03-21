<#
.SYNOPSIS
    Ignite Engine build entrypoint — runs inside the Windows Docker container.

.DESCRIPTION
    Invoked by `docker run` in GitHub Actions CI.
    Expects the repo workspace mounted at C:\workspace,
    with VULKAN_SDK, FBX_SDK_PATH, premake5.exe, and msbuild.exe available.
#>
param(
    [string]$Configuration = "Release",
    [string]$Platform      = "x64"
)

# Define workspace first — before anything else so it is never "unset"
$workspace = "C:\workspace"

$ErrorActionPreference = "Stop"
# NOTE: Set-StrictMode removed — it throws VariableIsUndefined for $workspace
#       in certain PowerShell startup code paths inside Docker containers.

# ## Banner #################################################################
Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  Ignite Engine - Docker Container Build"              -ForegroundColor Cyan
Write-Host "  Configuration : $Configuration"                      -ForegroundColor Cyan
Write-Host "  Platform      : $Platform"                           -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host ""

# ## Working directory ######################################################
if (-not (Test-Path $workspace)) {
    throw "Workspace not found at $workspace. Is the repo volume mounted with -v?"
}
Set-Location $workspace

# ## Verify environment #####################################################
Write-Host "[ENV] VULKAN_SDK   = $env:VULKAN_SDK"
Write-Host "[ENV] FBX_SDK_PATH = $env:FBX_SDK_PATH"

foreach ($bin in @("premake5.exe", "msbuild.exe")) {
    $found = Get-Command $bin -ErrorAction SilentlyContinue
    if (-not $found) {
        throw "$bin not found on PATH. Check the Docker image build."
    }
    Write-Host "[ENV] $bin = $($found.Source)"
}

if ($env:VULKAN_SDK -and -not (Test-Path $env:VULKAN_SDK)) {
    Write-Warning "VULKAN_SDK path not found: $env:VULKAN_SDK"
}
if ($env:FBX_SDK_PATH -and -not (Test-Path $env:FBX_SDK_PATH)) {
    Write-Warning "FBX_SDK_PATH not found: $env:FBX_SDK_PATH"
}

Write-Host ""

# ## Step 1: Premake ########################################################
Write-Host "[1/2] Generating project files with Premake..." -ForegroundColor Yellow
& premake5.exe --file=scripts/premake5.lua vs2022
if ($LASTEXITCODE -ne 0) {
    throw "Premake failed (exit $LASTEXITCODE)"
}
Write-Host "[1/2] Premake OK." -ForegroundColor Green
Write-Host ""

# ## Step 2: MSBuild ########################################################
Write-Host "[2/2] Building with MSBuild..." -ForegroundColor Yellow

$sln = Get-ChildItem $workspace -Filter "*.slnx" | Select-Object -First 1
if (-not $sln) {
    $sln = Get-ChildItem $workspace -Filter "*.sln" | Select-Object -First 1
}
if (-not $sln) {
    throw "No .slnx or .sln file found in $workspace"
}
Write-Host "[2/2] Solution: $($sln.Name)"

& msbuild.exe $sln.FullName `
    /p:Configuration=$Configuration `
    /p:Platform=$Platform `
    /m `
    /nologo `
    /consoleloggerparameters:Summary

if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed (exit $LASTEXITCODE)"
}

Write-Host "[2/2] Build SUCCEEDED." -ForegroundColor Green
Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  Build complete."                                      -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
