# ============================================================
# Ignite Engine – Windows Build Container
# Base: Windows Server Core LTSC 2022 (smaller than windows/server)
# Baked-in: MSVC Build Tools 17, Vulkan SDK 1.4.309.0,
#            FBX SDK 2020.3.7, Premake 5.0.0-beta8, Python 3
# ============================================================
FROM mcr.microsoft.com/windows/servercore:ltsc2022

SHELL ["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command"]

# ─── Versions (change here to upgrade) ──────────────────────
ARG VULKAN_VERSION=1.4.309.0
ARG FBX_SDK_VERSION=fbx202037
ARG PREMAKE_VERSION=5.0.0-beta8
ARG PYTHON_VERSION=3.12.9

# ─── Python ─────────────────────────────────────────────────
# Needed by setup scripts and CI steps
RUN $url = \"https://www.python.org/ftp/python/${env:PYTHON_VERSION}/python-${env:PYTHON_VERSION}-amd64.exe\"; \
    Write-Host \"[Python] Downloading $url\"; \
    Invoke-WebRequest -Uri $url -OutFile C:\python-installer.exe -UseBasicParsing; \
    Write-Host \"[Python] Installing...\"; \
    $p = Start-Process C:\python-installer.exe \
           -ArgumentList '/quiet InstallAllUsers=1 PrependPath=1 Include_test=0' \
           -NoNewWindow -PassThru -Wait; \
    if ($p.ExitCode -ne 0) { throw \"Python install failed: exit $($p.ExitCode)\" }; \
    Remove-Item C:\python-installer.exe -Force; \
    Write-Host \"[Python] Done.\"

# ─── MSVC Build Tools (C++ workload + MSBuild) ──────────────
# This layer is the heaviest (~5 GB); pin it first so it caches well.
RUN Write-Host \"[MSVC] Downloading Visual Studio Build Tools...\"; \
    Invoke-WebRequest \
      -Uri  \"https://aka.ms/vs/17/release/vs_buildtools.exe\" \
      -OutFile C:\vs_buildtools.exe \
      -UseBasicParsing; \
    Write-Host \"[MSVC] Installing (this takes several minutes)...\"; \
    $p = Start-Process C:\vs_buildtools.exe \
           -ArgumentList ( \
             '--quiet', '--wait', '--norestart', '--nocache', \
             '--installPath', 'C:\BuildTools', \
             '--add', 'Microsoft.VisualStudio.Workload.VCTools', \
             '--includeRecommended' \
           ) \
           -NoNewWindow -PassThru -Wait; \
    if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) { \
      throw \"VS Build Tools install failed: exit $($p.ExitCode)\" \
    }; \
    Remove-Item C:\vs_buildtools.exe -Force; \
    Write-Host \"[MSVC] Done.\"

# ─── Vulkan SDK ──────────────────────────────────────────────
RUN $url = \"https://sdk.lunarg.com/sdk/download/${env:VULKAN_VERSION}/windows/VulkanSDK-${env:VULKAN_VERSION}-Installer.exe\"; \
    Write-Host \"[Vulkan] Downloading $url\"; \
    Invoke-WebRequest -Uri $url -OutFile C:\vulkan-installer.exe -UseBasicParsing; \
    Write-Host \"[Vulkan] Installing...\"; \
    $p = Start-Process C:\vulkan-installer.exe \
           -ArgumentList '--accept-licenses', '--default-answer', '--confirm-command', 'install' \
           -NoNewWindow -PassThru -Wait; \
    if ($p.ExitCode -ne 0) { throw \"Vulkan SDK install failed: exit $($p.ExitCode)\" }; \
    Remove-Item C:\vulkan-installer.exe -Force; \
    Write-Host \"[Vulkan] Done.\"

# ─── FBX SDK 2020.3.7 ────────────────────────────────────────
RUN $url = \"https://damassets.autodesk.net/content/dam/autodesk/www/files/${env:FBX_SDK_VERSION}_fbxsdk_vs2022_win.exe\"; \
    Write-Host \"[FBX] Downloading $url\"; \
    Invoke-WebRequest -Uri $url -OutFile C:\fbx-installer.exe -UseBasicParsing; \
    Write-Host \"[FBX] Installing silently...\"; \
    $installDir = 'C:\FBX_SDK'; \
    Start-Process C:\fbx-installer.exe \
      -ArgumentList \"/S\", \"/D=$installDir\" \
      -NoNewWindow -Wait; \
    if (-not (Test-Path \"$installDir\")) { \
      Start-Process C:\fbx-installer.exe \
        -ArgumentList '/VERYSILENT', '/NORESTART', \"/DIR=$installDir\" \
        -NoNewWindow -Wait; \
    }; \
    $header = Get-ChildItem $installDir -Recurse -Filter fbxsdk.h -ErrorAction SilentlyContinue | Select-Object -First 1; \
    if (-not $header) { throw \"FBX SDK install failed: fbxsdk.h not found under $installDir\" }; \
    Remove-Item C:\fbx-installer.exe -Force; \
    Write-Host \"[FBX] Installed at $($header.DirectoryName -replace '\\\\include.*','')\"

# ─── Premake 5 ───────────────────────────────────────────────
RUN $url = \"https://github.com/premake/premake-core/releases/download/v${env:PREMAKE_VERSION}/premake-${env:PREMAKE_VERSION}-windows.zip\"; \
    Write-Host \"[Premake] Downloading $url\"; \
    Invoke-WebRequest -Uri $url -OutFile C:\premake.zip -UseBasicParsing; \
    New-Item -ItemType Directory -Path C:\tools\premake5 -Force | Out-Null; \
    Expand-Archive C:\premake.zip -DestinationPath C:\tools\premake5 -Force; \
    Remove-Item C:\premake.zip -Force; \
    if (-not (Test-Path 'C:\tools\premake5\premake5.exe')) { throw '[Premake] premake5.exe not found after extraction' }; \
    Write-Host \"[Premake] Done.\"

# ─── Environment Variables ────────────────────────────────────
ENV VULKAN_SDK="C:\VulkanSDK\1.4.309.0"
ENV FBX_SDK_PATH="C:\FBX_SDK\2020.3.7"

# Add MSBuild, Premake, and Vulkan glslc to PATH
RUN $current = [System.Environment]::GetEnvironmentVariable('PATH', 'Machine'); \
    $additions = @( \
        'C:\BuildTools\MSBuild\Current\Bin', \
        'C:\BuildTools\VC\Tools\MSVC\14.43.34808\bin\Hostx64\x64', \
        'C:\tools\premake5', \
        \"C:\VulkanSDK\${env:VULKAN_VERSION}\Bin\" \
    ) | Where-Object { $_ -notin ($current -split ';') }; \
    $newPath = ($current + ';' + ($additions -join ';')).TrimStart(';'); \
    [System.Environment]::SetEnvironmentVariable('PATH', $newPath, 'Machine'); \
    Write-Host \"[PATH] Updated.\"

# ─── Final verification ───────────────────────────────────────
RUN Write-Host \"--- Build container verification ---\"; \
    $msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue; \
    if ($msbuild) { Write-Host \"MSBuild: $($msbuild.Source)\" } else { Write-Warning \"MSBuild not found in PATH\" }; \
    $premake = Get-Command premake5.exe -ErrorAction SilentlyContinue; \
    if ($premake) { Write-Host \"Premake: $($premake.Source)\" } else { Write-Warning \"premake5.exe not found in PATH\" }; \
    Write-Host \"VULKAN_SDK = $env:VULKAN_SDK\"; \
    Write-Host \"FBX_SDK_PATH = $env:FBX_SDK_PATH\"; \
    Write-Host \"--- Done ---\"

WORKDIR C:\\workspace