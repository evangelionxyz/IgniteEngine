---
name: ignite-build-test
description: Build Ignite Engine (IGN.slnx) with CPU limits, regenerate Premake projects, sync HLSL shaders, verify Lua API stubs, and run the Ignite.Test.exe GoogleTest suite.
---

# Ignite Build, Shader Sync & Test Workflow

Use this skill whenever building C++ code, modifying HLSL shaders, adding/removing source files, editing Lua bindings, or verifying changes against `Ignite.Test.exe`.

## 1. Adding or Removing Source Files
Premake scans `src/**.cpp` and `src/**.hpp`. After creating, renaming, or deleting source files:
```powershell
D:\Dev\Ignite\scripts\downloads\premake5.exe --file=scripts/premake5.lua vs2026
```

## 2. Building Solution (`Debug|x64`)
Locate Visual Studio 2026 MSBuild via `vswhere` and cap parallelism (`/m:6 /p:CL_MPCount=6`, roughly half of max CPU threads on a 12-thread machine; avoid bare `/m`):
```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\amd64\MSBuild.exe" | Select-Object -First 1
& $msbuild D:\Dev\Ignite\IGN.slnx /p:Configuration=Debug /p:Platform=x64 /m:6 /p:CL_MPCount=6 /v:minimal
```
To build a single target quickly (e.g., runtime only):
```powershell
& $msbuild D:\Dev\Ignite\IGN.slnx /t:App\Ignite_Runtime /p:Configuration=Debug /p:Platform=x64 /m:6 /p:CL_MPCount=6 /v:minimal
```
Verify exit code `0` with `0 Error(s)`.

## 3. Syncing Updated Shaders (Required)
Binaries load shaders from `<cwd>/resources/shaders` (`bin/Debug/resources/shaders` when running from `bin/Debug`). Always sync from source after editing `.hlsl` or `.hlsli` files before running tests or executables:
```powershell
Copy-Item -Path D:\Dev\Ignite\ignite\editor\resources\shaders\* -Destination D:\Dev\Ignite\bin\Debug\resources\shaders\ -Recurse -Force
```

## 4. Verifying Lua API Stubs
Whenever adding or updating bindings in `ignite/engine/src/ignite/scripting/lua/lua.cpp`, update `ignite/editor/resources/lua/ignite.d.lua` and verify:
```powershell
python D:\Dev\Ignite\scripts\gen_lua_stubs.py --check
```

## 5. Running GoogleTest (`Ignite.Test.exe`)
Run `Ignite.Test.exe` from `D:\Dev\Ignite\bin\Debug` so relative `resources/` and `test-resources/` paths resolve cleanly:
```powershell
Push-Location D:\Dev\Ignite\bin\Debug
.\Ignite.Test.exe
$code = $LASTEXITCODE
Pop-Location
exit $code
```
Filter specific suites during iteration:
```powershell
.\Ignite.Test.exe --gtest_filter="AssetImporterUniqueNaming*:SceneHierarchy*:SceneDuplication*:UndoRedoSystem*:WidgetCanvasUndo*:ThumbnailsAndWatcher*"
```

### Test Authoring Hygiene
- Qualify `ignite::UUID` explicitly in tests that pull in Windows SDK headers (`vfs.hpp` -> `<Windows.h>`/`<rpc.h>` defines global `::UUID`), preventing `error C2872: 'UUID': ambiguous symbol`.
- Whenever a test calls `Project::Open(...)` or activates a `Project` with C# scripting, release local `Ref<Project>` handles and call `Project::CloseActive()` before the test finishes so HostFXR / CoreCLR shuts down cleanly prior to CRT static teardown.
