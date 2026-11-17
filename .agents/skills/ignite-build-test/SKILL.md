---
name: ignite-build-test
description: Build Ignite Engine (IGN.slnx), regenerate Premake projects, sync HLSL shaders to bin/Debug/resources/shaders, and run Ignite.Test.exe GoogleTest suite.
---

# Ignite Build, Shader Sync & Test Workflow

Use this skill whenever building C++ code, modifying HLSL shaders, adding new source files, or verifying changes against `Ignite.Test.exe`.

## 1. Adding/Removing Source Files
Premake scans `src/**.cpp` and `src/**.hpp`. After creating, renaming, or deleting source files:
```powershell
scripts\downloads\premake5.exe --file=scripts/premake5.lua vs2026
```

## 2. Building Solution (`Debug|x64`)
Find MSBuild cleanly or use `dotnet msbuild`:
```powershell
msbuild D:\Dev\Ignite\IGN.slnx /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```
To build a single target quickly (e.g., after editing runtime only):
```powershell
msbuild D:\Dev\Ignite\IGN.slnx /t:App\Ignite_Runtime /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

## 3. Syncing Updated Shaders (CRITICAL)
Binaries load shaders from `<cwd>/resources/shaders` (`bin/Debug/resources/shaders` when running binaries from `bin/Debug`). Always sync from source after editing `.hlsl`/`.hlsli` files:
```powershell
Copy-Item -Path D:\Dev\Ignite\ignite\editor\resources\shaders\* -Destination D:\Dev\Ignite\bin\Debug\resources\shaders\ -Recurse -Force
```

## 4. Running GoogleTest (`Ignite.Test.exe`)
Run the full test binary from `bin/Debug` (or via MCP `ignite_run_tests`):
```powershell
D:\Dev\Ignite\bin\Debug\Ignite.Test.exe
```
Filter a specific suite during iteration:
```powershell
D:\Dev\Ignite\bin\Debug\Ignite.Test.exe --gtest_filter=MeshBuilder*:*Lua*
```
