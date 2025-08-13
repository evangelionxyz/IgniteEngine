## Ignite Engine

Work in progress C++ Game Engine

## Build (CMake)

Prerequisites
- CMake 3.23+
- Git with submodules: clone with `--recursive`
- Windows: Visual Studio 2022 (Desktop C++), recommended Vulkan SDK (set `VULKAN_SDK` env)
- Linux: build-essential/clang/gcc, optionally Ninja, Vulkan drivers/SDK as needed

Clone
```bash
git clone --recursive https://github.com/evangelionxyz/Ignite.git
cd Ignite
```

Windows (PowerShell)
```powershell
./gen.bat
```
This configures CMake and builds Debug. To build again:
```powershell
cmake --build build --config Debug
```

Linux
```bash
chmod +x ./gen.sh
./gen.sh
```
This configures CMake and builds Debug. To build again (Multi-Config generators):
```bash
cmake --build build --config Debug
```
For single-config generators (e.g., Unix Makefiles):
```bash
cmake --build build
```

Advanced (cross-platform)
```bash
# Configure only (Release)
python scripts/setup.py -c Release

# Force generator (example: Ninja Multi-Config) and build
python scripts/setup.py -G "Ninja Multi-Config" --with-build -c Debug

# Clean and reconfigure
python scripts/setup.py --clean --with-build -c RelWithDebInfo
```

Open in IDE
- Windows: open `build/IGN.sln` in Visual Studio 2022 (startup project is `IgniteEditor`).

Run
- Windows: `build/bin/Debug/IgniteEditor.exe`
- Linux: `build/bin/Debug/IgniteEditor`

### Preview
<div style='display:flex;flex-direction:column;width:80%;margin:auto; gap:12px'>
  <img src="resources/examples/image_01.png">
  <img src="resources/examples/image_04.png">
  <img src="resources/examples/image_03.png">
  <img src="resources/examples/image_05.png">
  <img src="resources/examples/image_02.png">
</div>