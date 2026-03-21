# ============================================================
# Ignite Engine – Windows Build Container
# Base: Windows Server Core LTSC 2022 (smaller than windows/server)
# Baked-in: MSVC Build Tools 17, Vulkan SDK 1.4.309.0,
#            FBX SDK 2020.3.7, Premake 5.0.0-beta8, Python 3
# ============================================================
FROM mcr.microsoft.com/windows/servercore:ltsc2022

SHELL ["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command"]

#### Versions (change here to upgrade) #######
ARG VULKAN_VERSION=1.4.309.0
ARG FBX_SDK_VERSION=fbx202037
ARG PREMAKE_VERSION=5.0.0-beta8
ARG PYTHON_VERSION=3.12.9

#### Python #################################
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

#### MSVC Build Tools (C++ workload + C# Managed) ##
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
             '--add', 'Microsoft.VisualStudio.Workload.ManagedDesktopBuildTools', \
             '--includeRecommended' \
           ) \
           -NoNewWindow -PassThru -Wait; \
    if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) { \
      throw \"VS Build Tools install failed: exit $($p.ExitCode)\" \
    }; \
    Remove-Item C:\vs_buildtools.exe -Force; \
    Write-Host \"[MSVC] Done.\"

#### .NET SDK 8 #################################
# Required for SDK-style C# projects (.csproj with Microsoft.NET.Sdk).
# Installs dotnet CLI, SDK resolvers, and sets MSBuildSDKsPath dynamically.
RUN Write-Host \"[.NET] Installing .NET SDK 8.0...\"; \
    Invoke-WebRequest \
      -Uri \"https://dot.net/v1/dotnet-install.ps1\" \
      -OutFile C:\dotnet-install.ps1 \
      -UseBasicParsing; \
    & C:\dotnet-install.ps1 -Channel 8.0 -InstallDir C:\dotnet -NoPath; \
    Remove-Item C:\dotnet-install.ps1 -Force; \
    if (-not (Test-Path 'C:\dotnet\dotnet.exe')) { throw '[.NET] dotnet.exe not found after install' }; \
    $sdkVer = (Get-ChildItem 'C:\dotnet\sdk' -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name; \
    Write-Host \"[.NET] Detected SDK version: $sdkVer\"; \
    $sdksPath = \"C:\dotnet\sdk\$sdkVer\Sdks\"; \
    if (-not (Test-Path $sdksPath)) { throw \"[.NET] Sdks directory not found at $sdksPath\" }; \
    [System.Environment]::SetEnvironmentVariable('MSBuildSDKsPath', $sdksPath, 'Machine'); \
    Write-Host \"[.NET] MSBuildSDKsPath = $sdksPath\"; \
    Write-Host \"[.NET] Done.\"

ENV DOTNET_ROOT="C:\dotnet"

#### Vulkan SDK #################################
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

#### FBX SDK 2020.3.7 ###############################
# The NSIS installer with /D= puts files directly into $installDir.
# After install, we verify the expected structure and dynamically
# detect the actual SDK root (where include/fbxsdk.h lives).
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
    $fbxRoot = $header.DirectoryName -replace '[\\/]include.*',''; \
    Write-Host \"[FBX] SDK root detected at: $fbxRoot\"; \
    Write-Host \"[FBX] Contents:\" ; Get-ChildItem $fbxRoot | ForEach-Object { Write-Host \"  $_\" }; \
    [System.Environment]::SetEnvironmentVariable('FBX_SDK_PATH', $fbxRoot, 'Machine'); \
    Remove-Item C:\fbx-installer.exe -Force; \
    Write-Host \"[FBX] Done.\"

#### Premake 5 #################################
RUN $url = \"https://github.com/premake/premake-core/releases/download/v${env:PREMAKE_VERSION}/premake-${env:PREMAKE_VERSION}-windows.zip\"; \
    Write-Host \"[Premake] Downloading $url\"; \
    Invoke-WebRequest -Uri $url -OutFile C:\premake.zip -UseBasicParsing; \
    New-Item -ItemType Directory -Path C:\tools\premake5 -Force | Out-Null; \
    Expand-Archive C:\premake.zip -DestinationPath C:\tools\premake5 -Force; \
    Remove-Item C:\premake.zip -Force; \
    if (-not (Test-Path 'C:\tools\premake5\premake5.exe')) { throw '[Premake] premake5.exe not found after extraction' }; \
    Write-Host \"[Premake] Done.\"

#### Environment Variables ######################
ENV VULKAN_SDK="C:\VulkanSDK\1.4.309.0"
# FBX_SDK_PATH is set dynamically by the FBX installer RUN step above.
# This ENV is a fallback; the Machine-level env from the RUN takes precedence.
ENV FBX_SDK_PATH="C:\FBX_SDK"

#### Add MSBuild, Premake, Vulkan glslc, and .NET SDK to PATH
RUN $current = [System.Environment]::GetEnvironmentVariable('PATH', 'Machine'); \
    $msvcToolsRoot = 'C:\BuildTools\VC\Tools\MSVC'; \
    $msvcBin = if (Test-Path $msvcToolsRoot) { \
      $ver = Get-ChildItem $msvcToolsRoot | Sort-Object Name -Descending | Select-Object -First 1; \
      if ($ver) { Join-Path $ver.FullName 'bin\Hostx64\x64' } else { $null } \
    } else { $null }; \
    $additions = @( \
        'C:\BuildTools\MSBuild\Current\Bin', \
        'C:\dotnet', \
        'C:\tools\premake5', \
        \"C:\VulkanSDK\${env:VULKAN_VERSION}\Bin\" \
    ); \
    if ($msvcBin) { $additions += $msvcBin }; \
    $additions = $additions | Where-Object { $_ -and $_ -notin ($current -split ';') }; \
    $newPath = ($current + ';' + ($additions -join ';')).TrimStart(';'); \
    [System.Environment]::SetEnvironmentVariable('PATH', $newPath, 'Machine'); \
    Write-Host \"[PATH] Updated. MSVC bin: $msvcBin\"

#### Final verification #################################
RUN Write-Host \"--- Build container verification ---\"; \
    $msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue; \
    if ($msbuild) { Write-Host \"MSBuild: $($msbuild.Source)\" } else { Write-Warning \"MSBuild not found in PATH\" }; \
    $premake = Get-Command premake5.exe -ErrorAction SilentlyContinue; \
    if ($premake) { Write-Host \"Premake: $($premake.Source)\" } else { Write-Warning \"premake5.exe not found in PATH\" }; \
    $dotnet = Get-Command dotnet.exe -ErrorAction SilentlyContinue; \
    if ($dotnet) { Write-Host \"dotnet: $($dotnet.Source)\" } else { Write-Warning \"dotnet not found in PATH\" }; \
    Write-Host \"VULKAN_SDK      = $env:VULKAN_SDK\"; \
    Write-Host \"FBX_SDK_PATH    = $env:FBX_SDK_PATH\"; \
    Write-Host \"DOTNET_ROOT     = $env:DOTNET_ROOT\"; \
    Write-Host \"MSBuildSDKsPath = $([System.Environment]::GetEnvironmentVariable('MSBuildSDKsPath', 'Machine'))\"; \
    if (Test-Path $env:FBX_SDK_PATH) { \
      Write-Host \"FBX_SDK contents:\"; Get-ChildItem $env:FBX_SDK_PATH | ForEach-Object { Write-Host \"  $_\" } \
    } else { Write-Warning \"FBX_SDK_PATH directory missing!\" }; \
    Write-Host \"--- Done ---\"

WORKDIR C:\\workspace